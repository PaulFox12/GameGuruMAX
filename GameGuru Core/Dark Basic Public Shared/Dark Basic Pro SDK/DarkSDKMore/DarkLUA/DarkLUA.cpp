// DarkLUA.cpp : Defines the exported functions for the DLL application.
//

//#define FASTBULLETPHYSICS

#ifdef WINVER
#undef WINVER
#endif
//PE: We need the latest dpi functions.
#define WINVER 0x0605
#include "Windows.h"
#include "WinUser.h"

#define _USING_V110_SDK71_
#include "stdafx.h"
#include "DarkLUA.h"
#include "globstruct.h"
#include "CGfxC.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <unordered_set>
#include "M-RPG.h"

// DarkLUA needs access to the T global (but could be in two locations)
#include "..\..\..\..\GameGuru\Include\gameguru.h"

// Control of NAVMESH
#include "..\..\..\..\Guru-WickedMAX\GGRecastDetour\GGRecastDetour.h"
extern GGRecastDetour g_RecastDetour;

// Control of PARTICLES
#define NOTFORMAINENGINE
#include "..\..\..\..\Guru-WickedMAX\GPUParticles.h"
#undef NOTFORMAINENGINE

#ifdef STORYBOARD
#ifdef ENABLEIMGUI
#include "..\..\GameGuru\Imgui\imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "..\..\GameGuru\Imgui\imgui_internal.h"
#include "..\..\GameGuru\Imgui\imgui_impl_win32.h"
#include "..\..\GameGuru\Imgui\imgui_gg_dx11.h"
#endif
extern StoryboardStruct Storyboard;
#endif

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

#include "..\..\..\..\Guru-WickedMAX\wickedcalls.h"
#include "..\..\..\..\GameGuru\Include\GGPhysicsQuery.h"
#include "WickedEngine.h"
using namespace std;
using namespace wiGraphics;
using namespace wiScene;
using namespace wiECS;

#include "..\tracers\TracerManager.h"
using namespace Tracers;

#include "..\GGTerrain\GGGrass.h"
using namespace GGGrass;
#include "..\GGTerrain\GGTrees.h"

// Prototypes
extern void DrawSpritesFirst(void);
extern void DrawSpritesLast(void);

using namespace std;

//#define LUA_DO_DEBUG

#ifdef LUA_DO_DEBUG
void WriteToDebugLog ( char* szMessage, bool bNewLine );
void WriteToDebugLog ( char* szMessage, int i );
void WriteToDebugLog ( char* szMessage, float f );
void WriteToDebugLog ( char* szMessage, char* s );
#endif

#define lua_c

char errorString[256];
char functionName[256];
int functionParams = 0;
int functionResults = 0;
int functionStateID = 0;
int defaultState = 1;

DARKLUA_API int LoadLua( LPSTR pString );
bool LuaCheckForWorkshopFile ( LPSTR VirtualFilename);

 struct StringList
 {
	 int  stateID;
	 char fileName[MAX_PATH];
 };

 std::vector <StringList> ScriptsLoaded;
 std::vector <StringList> FunctionsWithErrors;

// Prototype function
float wrapangleoffset(float da);

// DarkAI Commands =======================================================================
HMODULE DarkAIModule = NULL;
HMODULE MultiplayerModule = NULL;

// externals to get tracking info from VR920 (if any)
//extern bool g_VR920AdapterAvailable;
//extern float g_fVR920TrackingYaw;
//extern float g_fVR920TrackingPitch;
//extern float g_fVR920TrackingRoll;
//extern float g_fDriverCompensationYaw;
//extern float g_fDriverCompensationPitch;
//extern float g_fDriverCompensationRoll;

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

 extern GlobStruct* g_pGlob;

 void LuaConstructor ( void )
 {
 }

 void LuaDestructor ( void )
 {
 }

 void LuaReceiveCoreDataPtr ( LPVOID pCore )
 {	
 }


struct luaState
{
	lua_State	*state;
};

lua_State *lua = NULL;

// Entity commands index t.entityelement directly, so an id outside it (such as an object number passed by mistake)
// wrote past the end of the array and crashed later somewhere unrelated. iMinID is 1, or 0 / -1 for the few commands
// that treat those as "no entity". Invalid ids are logged once per command and calling script line.
bool LuaEntityIDValid ( lua_State* L, int iEntityID, int iMinID )
{
	if ( iEntityID >= iMinID && iEntityID < (int)t.entityelement.size() ) return true;
	if ( g.gproducelogfiles > 0 )
	{
		static std::unordered_set<std::string> invalidEntityCallsLogged;

		// the call info only names functions called by their global name, so a command called through a local or
		// pcall is looked up in the globals table instead
		std::string name = "?";
		lua_Debug ar;
		if ( lua_getstack ( L, 0, &ar ) && lua_getinfo ( L, "nf", &ar ) )
		{
			if ( ar.name )
			{
				name = ar.name;
			}
			else
			{
				lua_pushglobaltable ( L );
				lua_pushnil ( L );
				while ( lua_next ( L, -2 ) )
				{
					if ( lua_type ( L, -2 ) == LUA_TSTRING && lua_rawequal ( L, -1, -4 ) )
					{
						name = lua_tostring ( L, -2 );
						lua_pop ( L, 2 );
						break;
					}
					lua_pop ( L, 1 );
				}
				lua_pop ( L, 1 );
			}
			lua_pop ( L, 1 );
		}

		// the first two script lines up the stack, skipping C frames such as pcall
		std::string where;
		int iScriptFrames = 0;
		for ( int level = 1; iScriptFrames < 2 && lua_getstack ( L, level, &ar ); level++ )
		{
			if ( lua_getinfo ( L, "Sl", &ar ) && ar.currentline > 0 )
			{
				char pFrame[300];
				snprintf ( pFrame, sizeof(pFrame), "%s%s:%d", iScriptFrames ? " < " : "", ar.short_src, ar.currentline );
				where += pFrame;
				iScriptFrames++;
			}
		}

		if ( invalidEntityCallsLogged.insert(name + where).second )
		{
			char pLog[1024];
			snprintf ( pLog, sizeof(pLog), "Lua %s ignored invalid entity %d (valid %d to %d) at %s", name.c_str(), iEntityID, iMinID, (int)t.entityelement.size()-1, where.c_str() );
			timestampactivity ( 0, pLog );
		}
	}
	return false;
}

int maxLuaStates = 0;
luaState** ppLuaStates = NULL;

//=============

struct luaMessage
{
	luaMessage() { strcpy ( msgDesc , "" ); msgIndex = 0; msgInt = 0; msgFloat = 0; strcpy ( msgString , "" ); }
	char msgDesc[256];
	int msgIndex;
	int msgInt;
	float msgFloat;
	char msgString[256];
};

luaMessage currentMessage;

int luaMessageCount = 0;
int maxLuaMessages = 0;
luaMessage** ppLuaMessages = NULL;

// extra debug info if runtime happens from a LUA script command call
thread_local lua_State* g_CurrentLuaState = nullptr;
//struct LuaStateGuard
//{
//	lua_State* prev;
//	LuaStateGuard(lua_State* L) : prev(g_CurrentLuaState) { g_CurrentLuaState = L; }
//	~LuaStateGuard() { g_CurrentLuaState = prev; }
//};
//inline int LuaGetTopAndSetState(lua_State* L)
//{
//	return lua_gettop(L);
//}
//#define LUA_BEGIN(L) LuaStateGuard _luaGuard(L)
//#define LUA_GETTOP(L) (LUA_BEGIN(L), lua_gettop(L))
static int LUA_GETTOP(lua_State* L)
{
	g_CurrentLuaState = L;
	return lua_gettop(L);
}

//=============

 int LuaSendMessage(lua_State *L)
 {
	 lua = L;

	/* get number of arguments */
	int n = LUA_GETTOP(L);
	int i;

	/* loop through each argument */
	for (i = 1; i <= n; i++)
	{
		luaMessageCount++;

		if ( maxLuaMessages == 0 )
		{
			strcpy ( currentMessage.msgDesc, "" );
			currentMessage.msgFloat = 0.0f;
			currentMessage.msgIndex = 0;
			currentMessage.msgInt = 0;
			strcpy ( currentMessage.msgString, "" );

			maxLuaMessages = 100;
			ppLuaMessages = new luaMessage*[maxLuaMessages];

			for ( int c = 0 ; c < maxLuaMessages ; c++ )
				ppLuaMessages[c] = NULL;
		}

		if ( luaMessageCount > maxLuaMessages )
		{
			luaMessage** ppBigger = NULL;
			ppBigger = new luaMessage*[luaMessageCount+100];

			for ( int c = 0; c < luaMessageCount; c++ )
			 ppBigger [ c ] = ppLuaMessages [ c ];

			delete [ ] ppLuaMessages;

			ppLuaMessages = ppBigger;

			for ( int c = maxLuaMessages; c < maxLuaMessages+100; c++ )
				ppLuaMessages[c] = NULL;

			maxLuaMessages += 100;
		}

		if ( ppLuaMessages[luaMessageCount-1] == NULL )
		{
	  
			luaMessage* msg = new luaMessage();

			strcpy ( msg->msgDesc , lua_tostring(L, i) );
			msg->msgIndex = 0;
			msg->msgFloat = 0;
			msg->msgInt = 0;
			strcpy ( msg->msgString , "" );
			ppLuaMessages[luaMessageCount-1] = msg;
		}
	}

	 return 0;
 }

 int LuaSendMessageI(lua_State *L)
 {
	 lua = L;

	/* get number of arguments */
	int n = LUA_GETTOP(L);
	int i;

	if ( n != 2 && n != 3 )
	{
		//MessageBox(NULL, "SendMessageI takes 2 or 3 params", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	luaMessageCount++;

	/* loop through each argument */
	for (i = 1; i <= n; i++)
	{
		if ( maxLuaMessages == 0 )
		{
			strcpy ( currentMessage.msgDesc, "" );
			currentMessage.msgIndex = 0;
			currentMessage.msgFloat = 0.0f;
			currentMessage.msgInt = 0;
			strcpy ( currentMessage.msgString, "" );

			maxLuaMessages = 100;
			ppLuaMessages = new luaMessage*[maxLuaMessages];

			for ( int c = 0 ; c < maxLuaMessages ; c++ )
				ppLuaMessages[c] = NULL;
		}

		if ( luaMessageCount > maxLuaMessages )
		{
			luaMessage** ppBigger = NULL;
			ppBigger = new luaMessage*[luaMessageCount+100];

			for ( int c = 0; c < luaMessageCount; c++ )
			 ppBigger [ c ] = ppLuaMessages [ c ];

			delete [ ] ppLuaMessages;

			ppLuaMessages = ppBigger;

			for ( int c = maxLuaMessages; c < maxLuaMessages+100; c++ )
				ppLuaMessages[c] = NULL;

			maxLuaMessages += 100;
		}

		if ( ppLuaMessages[luaMessageCount-1] == NULL )
		{
	  
			luaMessage* msg = new luaMessage();

			strcpy ( msg->msgDesc , lua_tostring(L, i) );
			msg->msgFloat = 0;
			msg->msgInt = 0;
			msg->msgIndex = 0;
			strcpy ( msg->msgString , "" );
			ppLuaMessages[luaMessageCount-1] = msg;

			/*if ( strcmp ( msg->msgDesc , "setentityhealth" ) == 0 )
			{
				int dave = 1;
			}*/
		}
		else
		{
			if ( n == 3 && i == 2 )
				ppLuaMessages[luaMessageCount-1]->msgIndex = (int)lua_tonumber( L , i );
			else
				ppLuaMessages[luaMessageCount-1]->msgInt = (int)lua_tonumber( L , i );
		}
	}

	 return 0;
 }

  int LuaSendMessageF(lua_State *L)
 {
	 lua = L;

	/* get number of arguments */
	int n = LUA_GETTOP(L);
	int i;

	if ( n != 2 && n != 3 )
	{
		//MessageBox(NULL, "SendMessageI takes 2 or 3 params", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	luaMessageCount++;

	/* loop through each argument */
	for (i = 1; i <= n; i++)
	{

		if ( maxLuaMessages == 0 )
		{
			strcpy ( currentMessage.msgDesc, "" );
			currentMessage.msgFloat = 0.0f;
			currentMessage.msgInt = 0;
			currentMessage.msgIndex = 0;
			strcpy ( currentMessage.msgString, "" );

			maxLuaMessages = 100;
			ppLuaMessages = new luaMessage*[maxLuaMessages];

			for ( int c = 0 ; c < maxLuaMessages ; c++ )
				ppLuaMessages[c] = NULL;
		}

		if ( luaMessageCount > maxLuaMessages )
		{
			luaMessage** ppBigger = NULL;
			ppBigger = new luaMessage*[luaMessageCount+100];

			for ( int c = 0; c < luaMessageCount; c++ )
			 ppBigger [ c ] = ppLuaMessages [ c ];

			delete [ ] ppLuaMessages;

			ppLuaMessages = ppBigger;

			for ( int c = maxLuaMessages; c < maxLuaMessages+100; c++ )
				ppLuaMessages[c] = NULL;

			maxLuaMessages += 100;
		}

		if ( ppLuaMessages[luaMessageCount-1] == NULL )
		{
	  
			luaMessage* msg = new luaMessage();

			strcpy ( msg->msgDesc , lua_tostring(L, i) );
			msg->msgFloat = 0;
			msg->msgInt = 0;
			msg->msgIndex = 0;
			strcpy ( msg->msgString , "" );
			ppLuaMessages[luaMessageCount-1] = msg;
		}
		else
		{

			if ( n == 3 && i == 2 )
				ppLuaMessages[luaMessageCount-1]->msgIndex = (int)lua_tonumber( L , i );
			else
				ppLuaMessages[luaMessageCount-1]->msgFloat = (float)lua_tonumber( L , i );
		}
	}

	 return 0;
 }

 int LuaSendMessageS(lua_State *L)
 {
	 lua = L;

	/* get number of arguments */
	int n = LUA_GETTOP(L);
	int i;

	if ( n != 2 && n != 3 )
	{
		//MessageBox(NULL, "SendMessageI takes 2 or 3 params", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	luaMessageCount++;

	/* loop through each argument */
	for (i = 1; i <= n; i++)
	{

		if ( maxLuaMessages == 0 )
		{
			strcpy ( currentMessage.msgDesc, "" );
			currentMessage.msgFloat = 0.0f;
			currentMessage.msgInt = 0;
			currentMessage.msgIndex = 0;
			strcpy ( currentMessage.msgString, "" );

			maxLuaMessages = 100;
			ppLuaMessages = new luaMessage*[maxLuaMessages];

			for ( int c = 0 ; c < maxLuaMessages ; c++ )
				ppLuaMessages[c] = NULL;
		}

		if ( luaMessageCount > maxLuaMessages )
		{
			luaMessage** ppBigger = NULL;
			ppBigger = new luaMessage*[luaMessageCount+100];

			for ( int c = 0; c < luaMessageCount; c++ )
			 ppBigger [ c ] = ppLuaMessages [ c ];

			delete [ ] ppLuaMessages;

			ppLuaMessages = ppBigger;

			for ( int c = maxLuaMessages; c < maxLuaMessages+100; c++ )
				ppLuaMessages[c] = NULL;

			maxLuaMessages += 100;
		}

		if ( ppLuaMessages[luaMessageCount-1] == NULL )
		{
	  
			luaMessage* msg = new luaMessage();

			strcpy ( msg->msgDesc , lua_tostring(L, i) );
			msg->msgFloat = 0;
			msg->msgInt = 0;
			msg->msgIndex = 0;
			strcpy ( msg->msgString , "" );
			ppLuaMessages[luaMessageCount-1] = msg;
		}
		else
		{
			if ( n == 3 && i == 2 )
				ppLuaMessages[luaMessageCount-1]->msgIndex = (int)lua_tonumber( L , i );
			else
			{
				// ZJ: Prevent copying a nullptr 
				const char* argument = lua_tostring(L, i);
				if (argument)
				{
					strcpy(ppLuaMessages[luaMessageCount - 1]->msgString, argument);
				}
				//strcpy(ppLuaMessages[luaMessageCount - 1]->msgString, lua_tostring(L, i));
			}
		}
	}

	 return 0;
 }

 // Direct Calls
 void lua_updateweaponstats ( void );

 int RestoreGameFromSlot(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	t.luaglobal.gamestatechange = lua_tonumber(L, 1);
	if ( t.luaglobal.gamestatechange==0 )
	{
		// if successfully reset a game-load-state, also ensure advance level loader resets
		strcpy ( t.game.pAdvanceWarningOfLevelFilename, "" );
	}
	return 0;
 }
 int ResetFade(lua_State *L)
 {
	lua = L;
	if ( t.game.gameloop == 1 )
	{
		// only blank if in the game menu (not main menu load page)
		postprocess_reset_fade();
		DisableAllSprites();
		Sync();
		Sync();
	}
	return 0;
 }

 int GetInternalSoundState(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iIndex = lua_tonumber(L, 1);
	if (iIndex >= 0 && iIndex < 65535)
	{
		int iState = t.soundloopcheckpoint[iIndex];
		lua_pushinteger ( L, iState );
	}
	return 1;
 }
 int SetInternalSoundState(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iIndex = lua_tonumber(L, 1);
	if (iIndex >= 0 && iIndex < 65535)
	{
		int iState = lua_tonumber(L, 2);
		t.soundloopcheckpoint[iIndex] = iState;
		if (t.soundloopcheckpoint[iIndex] != 0)
		{
			if (iIndex > 0 && SoundExist(iIndex) == 1)
			{
				if (t.soundloopcheckpoint[iIndex] == 3)
					LoopSound(iIndex);
				else if (t.soundloopcheckpoint[iIndex] == 1)
					PlaySound(iIndex);
			}
		}
		return 1;
	}
	return 0;
 }
 int SetCheckpoint(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	t.playercheckpoint.x=lua_tonumber(L, 1);
	t.playercheckpoint.y=lua_tonumber(L, 2);
	t.playercheckpoint.z=lua_tonumber(L, 3);
	t.playercheckpoint.a=lua_tonumber(L, 4);
	return 1;
 }

 int UpdateWeaponStats(lua_State *L)
 {
	lua = L;
	lua_updateweaponstats();
	return 0;
 }
  
 int ResetWeaponSystems(lua_State *L)
 {
	weapon_projectile_free();
	return 0;
 }
 int GetWeaponSlotGot(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iWeaponSlot = lua_tonumber(L, 1);
	lua_pushinteger ( L, t.weaponslot[iWeaponSlot].got );
	return 1;
 }
 int GetWeaponSlotNoSelect(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iWeaponSlot = lua_tonumber(L, 1);
	lua_pushinteger ( L, t.weaponslot[iWeaponSlot].noselect );
	return 1;
 }
 int SetWeaponSlot(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	int iWeaponSlot = lua_tonumber(L, 1);
	t.weaponslot[iWeaponSlot].got = lua_tonumber(L, 2);
	t.weaponslot[iWeaponSlot].pref = lua_tonumber(L, 3);
	return 0;
 }
 int GetWeaponAmmo(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iWeaponSlot = lua_tonumber(L, 1);
	if(iWeaponSlot>=0 && iWeaponSlot<t.weaponammo.size())
		lua_pushinteger ( L, t.weaponammo[iWeaponSlot] );
	else
		lua_pushinteger ( L, 0 );
	return 1;
 }
 int SetWeaponAmmo(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iWeaponSlot = lua_tonumber(L, 1);
	if (iWeaponSlot >= 0 && iWeaponSlot < t.weaponammo.size())
	{
		t.weaponammo[iWeaponSlot] = lua_tonumber(L, 2);
	}
	return 0;
 }
 int GetWeaponClipAmmo(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iWeaponSlotClipIndex = lua_tonumber(L, 1);
	if (iWeaponSlotClipIndex >= 0 && iWeaponSlotClipIndex < t.weaponclipammo.size())
		lua_pushinteger ( L, t.weaponclipammo[iWeaponSlotClipIndex] );
	else
		lua_pushinteger ( L, 0 );
	return 1;
 }
 int SetWeaponClipAmmo(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iWeaponSlotClipIndex = lua_tonumber(L, 1);
	if (iWeaponSlotClipIndex >= 0 && iWeaponSlotClipIndex < t.weaponclipammo.size())
	{
		t.weaponclipammo[iWeaponSlotClipIndex] = lua_tonumber(L, 2);
		int iWeaponSlot = iWeaponSlotClipIndex;
		if (iWeaponSlot >= 11) iWeaponSlot = iWeaponSlot - 10;
		int iGunIndex = t.weaponslot[iWeaponSlot].got;
		int iMaxClipCapacity = g.firemodes[iGunIndex][0].settings.clipcapacity * g.firemodes[iGunIndex][0].settings.reloadqty;
		if (iMaxClipCapacity == 0) iMaxClipCapacity = 99999;
		if (t.weaponclipammo[iWeaponSlotClipIndex] > iMaxClipCapacity) t.weaponclipammo[iWeaponSlotClipIndex] = iMaxClipCapacity;
	}
	return 0;
 }
 int GetWeaponPoolAmmoIndex(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iWeaponSlot = lua_tonumber(L, 1);
	 if (iWeaponSlot >= 0 && iWeaponSlot < t.weaponslot.size())
	 {
		 int iGunIndex = t.weaponslot[iWeaponSlot].got;
		 int iPoolIndex = g.firemodes[iGunIndex][g.firemode].settings.poolindex;
		 lua_pushinteger (L, iPoolIndex);
	 }
	 else
	 {
		 lua_pushinteger (L, 0);
	 }
	 return 1;
 }
 int GetWeaponPoolAmmo(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iPoolIndex = lua_tonumber(L, 1);
	if (iPoolIndex >= 0 && iPoolIndex < t.ammopool.size())
	{
		lua_pushinteger (L, t.ammopool[iPoolIndex].ammo);
	}
	else
	{
		lua_pushinteger (L, 0);	
	}
	return 1;
 }
 int SetWeaponPoolAmmo(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iPoolIndex = lua_tonumber(L, 1);
	if (iPoolIndex >= 0 && iPoolIndex < t.ammopool.size())
	{
		t.ammopool[iPoolIndex].ammo = lua_tonumber(L, 2);
	}
	return 0;
 }
 int GetWeaponSlot(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;

	// returns the gunID
	int iReturnValue = 0;

	// find gun name specicied to get gunindex
	int iWeaponSlot = lua_tonumber(L, 1);
	iReturnValue = t.weaponslot[iWeaponSlot].got;

	// return true GunID found in slot
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetWeaponSlotPref(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iWeaponID = 0;
	 int iWeaponSlot = lua_tonumber(L, 1);
	 iWeaponID = t.weaponslot[iWeaponSlot].pref;
	 lua_pushinteger (L, iWeaponID);
	 return 1;
 }

 // Weapon Modding Commands
 int GetPlayerWeaponID(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n > 0 ) return 0;

	// returns the playres current gun ID
	int iReturnValue = t.gunid;
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetWeaponID(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;

	// returns the gun
	int iReturnValue = 0;

	// find gun name specicied to get gunindex
	char pGunName[512];
	strcpy ( pGunName, lua_tostring(L, 1) );
	for ( int tgunid = 1; tgunid < ArrayCount(t.gun); tgunid++ )
	{
		if ( stricmp ( t.gun[tgunid].name_s.Get()+(strlen(t.gun[tgunid].name_s.Get())-strlen(pGunName)), pGunName ) == NULL )
		{
			iReturnValue = tgunid;
			break;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetEntityWeaponID(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iEntityBankIndex = t.entityelement[iEntityIndex].bankindex;
		if ( iEntityBankIndex > 0 )
		{
			iReturnValue = t.entityprofile[iEntityBankIndex].isweapon;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int RawSetWeaponData ( lua_State *L, int iDataMode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	int tgunid = lua_tonumber(L, 1);
	int tfiremode = lua_tonumber(L, 2);
	int newvalue = lua_tonumber(L, 3);
	if ( tgunid > 0 && tgunid <= ArrayCount(t.gun) )
	{
		switch ( iDataMode )
		{
			case 1 : g.firemodes[tgunid][tfiremode].settings.damage = newvalue; break;
			case 2 : g.firemodes[tgunid][tfiremode].settings.accuracy = newvalue; break;
			case 3 : g.firemodes[tgunid][tfiremode].settings.reloadqty = newvalue; break;
			case 4 : g.firemodes[tgunid][tfiremode].settings.iterate = newvalue; break;
			case 5 : g.firemodes[tgunid][tfiremode].settings.range = newvalue; break;
			case 6 : g.firemodes[tgunid][tfiremode].settings.dropoff = newvalue; break;
			case 7 : g.firemodes[tgunid][tfiremode].settings.usespotlighting = newvalue; break;
			case 8 : g.firemodes[tgunid][tfiremode].settings.firerate = newvalue; break;
			case 9 : g.firemodes[tgunid][tfiremode].settings.clipcapacity = newvalue; break;
			case 10: g.firemodes[tgunid][tfiremode].settings.weaponpropres1 = newvalue; break;
			case 11: g.firemodes[tgunid][tfiremode].settings.weaponpropres2 = newvalue; break;
		}
	}
	return 0;
 }
 int RawGetWeaponData( lua_State *L, int iDataMode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;

	// specify weaponID and firemode index
	int tgunid = lua_tonumber(L, 1);
	int tfiremode = lua_tonumber(L, 2);

	// returns the field data
	int iReturnValue = 0;

	// use datamode to determine which data is returned
	if ( tgunid > 0 && tgunid <= ArrayCount(t.gun) )
	{
		switch ( iDataMode )
		{
			case 1 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.damage; break;
			case 2 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.accuracy; break;
			case 3 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.reloadqty; break;
			case 4 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.iterate; break;
			case 5 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.range; break;
			case 6 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.dropoff; break;
			case 7 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.usespotlighting; break;
			case 8 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.firerate; break;
			case 9 : iReturnValue = g.firemodes[tgunid][tfiremode].settings.clipcapacity; break;
			case 10: iReturnValue = g.firemodes[tgunid][tfiremode].settings.weaponpropres1; break;
			case 11: iReturnValue = g.firemodes[tgunid][tfiremode].settings.weaponpropres2; break;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetWeaponName(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int tgunid = lua_tonumber(L, 1);
	 if(tgunid>0)
		 lua_pushstring (L, t.gun[tgunid].name_s.Get());
	 else
		 lua_pushstring (L, "");
	 return 1;
 }
 int SetWeaponDamage(lua_State *L) { return RawSetWeaponData ( L, 1 ); }
 int SetWeaponAccuracy(lua_State *L) { return RawSetWeaponData ( L, 2 ); }
 int SetWeaponReloadQuantity(lua_State *L) { return RawSetWeaponData ( L, 3 ); }
 int SetWeaponFireIterations(lua_State *L) { return RawSetWeaponData ( L, 4 ); }
 int SetWeaponRange(lua_State *L) { return RawSetWeaponData ( L, 5 ); }
 int SetWeaponDropoff(lua_State *L) { return RawSetWeaponData ( L, 6 ); }
 int SetWeaponSpotLighting(lua_State *L) { return RawSetWeaponData ( L, 7 ); }
 int GetWeaponDamage(lua_State *L) { return RawGetWeaponData ( L, 1 ); }
 int GetWeaponAccuracy(lua_State *L) { return RawGetWeaponData ( L, 2 ); }
 int GetWeaponReloadQuantity(lua_State *L) { return RawGetWeaponData ( L, 3 ); }
 int GetWeaponFireIterations(lua_State *L) { return RawGetWeaponData ( L, 4 ); }
 int GetWeaponRange(lua_State *L) { return RawGetWeaponData ( L, 5 ); }
 int GetWeaponDropoff(lua_State *L) { return RawGetWeaponData ( L, 6 ); }
 int GetWeaponSpotLighting(lua_State *L) { return RawGetWeaponData ( L, 7 ); }
 int SetWeaponFireRate(lua_State* L) { return RawSetWeaponData (L, 8); }
 int GetWeaponFireRate(lua_State* L) { return RawGetWeaponData (L, 8); }
 int SetWeaponClipCapacity(lua_State* L) { return RawSetWeaponData (L, 9); }
 int GetWeaponClipCapacity(lua_State* L) { return RawGetWeaponData (L, 9); }
 //int SetWeaponWeaponPropRes1(lua_State* L) { return RawSetWeaponData (L, 10); } reserved
 //int GetWeaponWeaponPropRes1(lua_State* L) { return RawGetWeaponData (L, 10); }
 //int SetWeaponWeaponPropRes2(lua_State* L) { return RawSetWeaponData (L, 11); }
 //int GetWeaponWeaponPropRes2(lua_State* L) { return RawGetWeaponData (L, 11); }

 //
 // Player Camera Overrides
 //
 int RawSetCameraData ( lua_State *L, int iDataMode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	int tcameraid=0, tvalue=0;
	float fX=0, fY=0, fZ=0;
	if ( iDataMode < 11 )
	{
		if ( n < 1 ) return 0;
		tvalue = lua_tonumber(L, 1);
	}
	else
	{
		if ( n < 4 ) return 0;
		tcameraid = lua_tonumber(L, 1);
		fX = lua_tonumber(L, 2);
		fY = lua_tonumber(L, 3);
		fZ = lua_tonumber(L, 4);
	}
	switch ( iDataMode )
	{
		case 1 : g.luacameraoverride = tvalue; break;
		case 11 : if ( tcameraid == 0 ) { PositionCamera ( tcameraid, fX, fY, fZ ); } break;
		case 12 : if ( tcameraid == 0 ) 
		{ 
			RotateCamera ( tcameraid, fX, fY, fZ ); 
		} 
		break;
		case 13 : if ( tcameraid == 0 ) 
		{
			RotateCamera ( tcameraid, 0, 0, 0 );
			RollCameraRight ( tcameraid, fZ );
			TurnCameraRight ( tcameraid, fY );
			PitchCameraUp ( tcameraid, fX );
		} 
		break;
	}
	return 0;
 }
 int RawGetCameraData( lua_State *L, int iDataMode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	int tcameraid = 0;
	if ( iDataMode < 500 )
	{
		if ( n < 1 ) return 0;
		tcameraid = lua_tonumber(L, 1);
	}
	float fReturnValue = 0;
	int iReturnValue = 0;
	if ( tcameraid == 0 )
	{
		switch ( iDataMode )
		{
			case 1 : fReturnValue = CameraPositionX ( tcameraid ); break;
			case 2 : fReturnValue = CameraPositionY ( tcameraid ); break;
			case 3 : fReturnValue = CameraPositionZ ( tcameraid ); break;
			case 4 : fReturnValue = CameraAngleX ( tcameraid ); break;
			case 5 : fReturnValue = CameraAngleY ( tcameraid ); break;
			case 6 : fReturnValue = CameraAngleZ ( tcameraid ); break;
			case 7: fReturnValue = GetCameraLook().x; break;
			case 8: fReturnValue = GetCameraLook().y; break;
			case 9: fReturnValue = GetCameraLook().z; break;
			case 501 : iReturnValue = g.luacameraoverride; break;
		}
	}
	if ( iDataMode < 500 )
		lua_pushnumber ( L, fReturnValue );
	else
		lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int WrapAngle(lua_State *L) 
 { 
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float fAngle = wrapangleoffset(lua_tonumber(L, 1));
	float fDestAngle = wrapangleoffset(lua_tonumber(L, 2));
	float fSmoothFactor = lua_tonumber(L, 3);
	float fReturnValue = fAngle;
	float fDiff = (fDestAngle-fAngle);
	if ( fDiff > 180.0f ) fDiff -= 360.0f;
	if ( fDiff < -180.0f ) fDiff += 360.0f;
	fReturnValue += fDiff*fSmoothFactor;
	lua_pushinteger ( L, fReturnValue );
	return 1;
 }
 int GetCameraOverride(lua_State *L) { return RawGetCameraData ( L, 501 ); }
 int SetCameraOverride(lua_State *L) { return RawSetCameraData ( L, 1 ); }
 int SetCameraPosition(lua_State *L) { return RawSetCameraData ( L, 11 ); }
 int SetCameraAngle(lua_State *L) { return RawSetCameraData ( L, 12 ); }
 int SetCameraFreeFlight(lua_State *L) { return RawSetCameraData ( L, 13 ); }
 int GetCameraPositionX(lua_State *L) { return RawGetCameraData ( L, 1 ); }
 int GetCameraPositionY(lua_State *L) { return RawGetCameraData ( L, 2 ); }
 int GetCameraPositionZ(lua_State *L) { return RawGetCameraData ( L, 3 ); }
 int GetCameraAngleX(lua_State *L) { return RawGetCameraData ( L, 4 ); }
 int GetCameraAngleY(lua_State *L) { return RawGetCameraData ( L, 5 ); }
 int GetCameraAngleZ(lua_State *L) { return RawGetCameraData ( L, 6 ); }
 int GetCameraLookAtX(lua_State* L) { return RawGetCameraData(L, 7); }
 int GetCameraLookAtY(lua_State* L) { return RawGetCameraData(L, 8); }
 int GetCameraLookAtZ(lua_State* L) { return RawGetCameraData(L, 9); }

 int SetCameraFOV ( lua_State *L )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iCameraIndex = lua_tonumber(L, 1);
	float fCameraFOV = lua_tonumber(L, 2);
	SetCameraFOV ( iCameraIndex, fCameraFOV );
	return 0;
 }

 //
 // Player Direct commands
 //
 int RawSetPlayerData ( lua_State *L, int iDataMode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( iDataMode == 1 )
	{
 		//  apply force to push player
		if ( n < 2 ) return 0;
		float fAngle = lua_tonumber(L, 1);
		float fForce = lua_tonumber(L, 2);
		t.playercontrol.pushangle_f = fAngle;
		t.playercontrol.pushforce_f = fForce;
	}
	return 0;
 }
 int ForcePlayer(lua_State *L) { return RawSetPlayerData ( L, 1 ); }

 //
 // All SET & GET Functions which replaces g_Entity[e] tables
 //
 /*
 int SetEntityLUACore ( lua_State *L, int iCode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iIndex = lua_tonumber(L, 1);
	if ( !LuaEntityIDValid ( L, iIndex, 1 ) ) return 0;
	switch ( iCode )
	{
		case 1 : t.entityelement[iIndex].luadata.x = lua_tonumber(L, 2); break;
		case 2 : t.entityelement[iIndex].luadata.y = lua_tonumber(L, 2); break;
		case 3 : t.entityelement[iIndex].luadata.z = lua_tonumber(L, 2); break;
		case 4 : t.entityelement[iIndex].luadata.anglex = lua_tonumber(L, 2); break;
		case 5 : t.entityelement[iIndex].luadata.angley = lua_tonumber(L, 2); break;
		case 6 : t.entityelement[iIndex].luadata.anglez = lua_tonumber(L, 2); break;
		case 7 : t.entityelement[iIndex].luadata.obj = lua_tonumber(L, 2); break;
		case 8 : t.entityelement[iIndex].luadata.active = lua_tonumber(L, 2); break;
		case 9 : t.entityelement[iIndex].luadata.activated = lua_tonumber(L, 2); break;
		case 10 : t.entityelement[iIndex].luadata.collected = lua_tonumber(L, 2); break;
		case 11 : t.entityelement[iIndex].luadata.haskey = lua_tonumber(L, 2); break;
		case 12 : t.entityelement[iIndex].luadata.plrinzone = lua_tonumber(L, 2); break;
		case 13 : t.entityelement[iIndex].luadata.entityinzone = lua_tonumber(L, 2); break;
		case 14 : t.entityelement[iIndex].luadata.plrvisible = lua_tonumber(L, 2); break;
		case 15 : t.entityelement[iIndex].luadata.health = lua_tonumber(L, 2); break;
		case 16 : t.entityelement[iIndex].luadata.frame = lua_tonumber(L, 2); break;
		case 17 : t.entityelement[iIndex].luadata.timer = lua_tonumber(L, 2); break;
		case 18 : t.entityelement[iIndex].luadata.plrdist = lua_tonumber(L, 2); break;
		case 19 : t.entityelement[iIndex].luadata.avoid = lua_tonumber(L, 2); break;
		case 20 : t.entityelement[iIndex].luadata.limbhit = lua_tostring(L, 2); break;
		case 21 : t.entityelement[iIndex].luadata.limbhitindex = lua_tonumber(L, 2); break;
		case 22 : t.entityelement[iIndex].luadata.animating = lua_tonumber(L, 2); break;
	}
	return 0;
 }
 int GetEntityLUACore ( lua_State *L, int iCode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iIndex = lua_tonumber(L, 1);
	if ( LuaEntityIDValid ( L, iIndex, 1 ) )
	{
		switch ( iCode )
		{
			case 1 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.x ); break;
			case 2 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.y ); break;
			case 3 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.z ); break;
			case 4 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.anglex ); break;
			case 5 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.angley ); break;
			case 6 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.anglez ); break;
			case 7 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.obj ); break;
			case 8 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.active ); break;
			case 9 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.activated ); break;
			case 10 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.collected ); break;
			case 11 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.haskey ); break;
			case 12 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.plrinzone ); break;
			case 13 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.entityinzone ); break;
			case 14 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.plrvisible ); break;
			case 15 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.health ); break;
			case 16 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.frame ); break;
			case 17 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.timer ); break;
			case 18 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.plrdist ); break;
			case 19 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.avoid ); break;
			case 20 : lua_pushstring ( L, t.entityelement[iIndex].luadata.limbhit.Get() ); break;
			case 21 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.limbhitindex ); break;
			case 22 : lua_pushinteger ( L, t.entityelement[iIndex].luadata.animating ); break;
		}
		return 1;
	}
	else
	{
		// return zero if no index specified (error)
		return 0;
	}
 }

 int SetEntityLUAX(lua_State *L) { return SetEntityLUACore ( L, 1 ); }
 int GetEntityLUAX(lua_State *L) { return GetEntityLUACore ( L, 1 ); }
 int SetEntityLUAY(lua_State *L) { return SetEntityLUACore ( L, 2 ); }
 int GetEntityLUAY(lua_State *L) { return GetEntityLUACore ( L, 2 ); }
 int SetEntityLUAZ(lua_State *L) { return SetEntityLUACore ( L, 3 ); }
 int GetEntityLUAZ(lua_State *L) { return GetEntityLUACore ( L, 3 ); }
 int SetEntityLUAAngleX(lua_State *L) { return SetEntityLUACore ( L, 4 ); }
 int GetEntityLUAAngleX(lua_State *L) { return GetEntityLUACore ( L, 4 ); }
 int SetEntityLUAAngleY(lua_State *L) { return SetEntityLUACore ( L, 5 ); }
 int GetEntityLUAAngleY(lua_State *L) { return GetEntityLUACore ( L, 5 ); }
 int SetEntityLUAAngleZ(lua_State *L) { return SetEntityLUACore ( L, 6 ); }
 int GetEntityLUAAngleZ(lua_State *L) { return GetEntityLUACore ( L, 6 ); }
 int SetEntityLUAObj(lua_State *L) { return SetEntityLUACore ( L, 7 ); }
 int GetEntityLUAObj(lua_State *L) { return GetEntityLUACore ( L, 7 ); }
 int SetEntityLUAActive(lua_State *L) { return SetEntityLUACore ( L, 8 ); }
 int GetEntityLUAActive(lua_State *L) { return GetEntityLUACore ( L, 8 ); }
 int SetEntityLUAActivated(lua_State *L) { return SetEntityLUACore ( L, 9 ); }
 int GetEntityLUAActivated(lua_State *L) { return GetEntityLUACore ( L, 9 ); }
 int SetEntityLUACollected(lua_State *L) { return SetEntityLUACore ( L, 10 ); }
 int GetEntityLUACollected(lua_State *L) { return GetEntityLUACore ( L, 10 ); }
 int SetEntityLUAHasKey(lua_State *L) { return SetEntityLUACore ( L, 11 ); }
 int GetEntityLUAHasKey(lua_State *L) { return GetEntityLUACore ( L, 11 ); }
 int SetEntityLUAPlrInZone(lua_State *L) { return SetEntityLUACore ( L, 12 ); }
 int GetEntityLUAPlrInZone(lua_State *L) { return GetEntityLUACore ( L, 12 ); }
 int SetEntityLUAEntityInZone(lua_State *L) { return SetEntityLUACore ( L, 13 ); }
 int GetEntityLUAEntityInZone(lua_State *L) { return GetEntityLUACore ( L, 13 ); }
 int SetEntityLUAPlrVisible(lua_State *L) { return SetEntityLUACore ( L, 14 ); }
 int GetEntityLUAPlrVisible(lua_State *L) { return GetEntityLUACore ( L, 14 ); }
 int SetEntityLUAAnimating(lua_State *L) { return SetEntityLUACore ( L, 22 ); }
 int GetEntityLUAAnimating(lua_State *L) { return GetEntityLUACore ( L, 22 ); }
 int SetEntityLUAHealth(lua_State *L) { return SetEntityLUACore ( L, 15 ); }
 int GetEntityLUAHealth(lua_State *L) { return GetEntityLUACore ( L, 15 ); }
 int SetEntityLUAFrame(lua_State *L) { return SetEntityLUACore ( L, 16 ); }
 int GetEntityLUAFrame(lua_State *L) { return GetEntityLUACore ( L, 16 ); }
 int SetEntityLUATimer(lua_State *L) { return SetEntityLUACore ( L, 17 ); }
 int GetEntityLUATimer(lua_State *L) { return GetEntityLUACore ( L, 17 ); }
 int SetEntityLUAPlrDist(lua_State *L) { return SetEntityLUACore ( L, 18 ); }
 int GetEntityLUAPlrDist(lua_State *L) { return GetEntityLUACore ( L, 18 ); }
 int SetEntityLUAAvoid(lua_State *L) { return SetEntityLUACore ( L, 19 ); }
 int GetEntityLUAAvoid(lua_State *L) { return GetEntityLUACore ( L, 19 ); }
 int SetEntityLUALimbHit(lua_State *L) { return SetEntityLUACore ( L, 20 ); }
 int GetEntityLUALimbHit(lua_State *L) { return GetEntityLUACore ( L, 20 ); }
 int SetEntityLUALimbHitIndex(lua_State *L) { return SetEntityLUACore ( L, 21 ); }
 int GetEntityLUALimbHitIndex(lua_State *L) { return GetEntityLUACore ( L, 21 ); }
 */

 // Direct Entity Element Instructions (different from LUA DATA instructions above)

 int SetEntityActive(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iIndex = lua_tonumber(L, 1);
	int iSetThisValue = lua_tonumber(L, 2);
	if ( !LuaEntityIDValid ( L, iIndex, 1 ) ) return 0;
	t.entityelement[iIndex].active = iSetThisValue;
	return 0;
 }
 int SetEntityActivated(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iIndex = lua_tonumber(L, 1);
	if ( !LuaEntityIDValid ( L, iIndex, 1 ) ) return 0;
	t.entityelement[iIndex].activated = lua_tonumber(L, 2);
	return 0;
 }
 int SetEntityHasKey(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iIndex = lua_tonumber(L, 1);
	 if (!LuaEntityIDValid(L, iIndex, 1)) return 0;
	 t.entityelement[iIndex].lua.haskey = lua_tonumber(L, 2);
	 return 0;
 }
 int SetEntityObjective(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iIndex = lua_tonumber(L, 1);
	 if (!LuaEntityIDValid(L, iIndex, 1)) return 0;
	 t.entityelement[iIndex].eleprof.isobjective = lua_tonumber(L, 2);
	 return 0;
 }
 int SetEntityCollectable(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iIndex = lua_tonumber(L, 1);
	 if (!LuaEntityIDValid(L, iIndex, 1)) return 0;
	 t.entityelement[iIndex].eleprof.iscollectable = lua_tonumber(L, 2);
	 return 0;
 }
 int SetEntityCollectedEx(lua_State *L, bool bForceMode)
 {
	// bForceMode when true will ignore state of entity, only interested in adding to inventory (used for saved game restoring)
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 || n > 6 ) return 0;
	int iReturnSlot = -1;
	bool bItemHandled = false;
	bool bRemoveEntityFromGame = false;
	int iEntityIndex = lua_tonumber(L, 1);
	int iCollectState = lua_tonumber(L, 2);
	// zero is allowed (fills inventory slots when a saved game loads), and every use below is inside iEntityIndex > 0
	if (iEntityIndex > 0 && !LuaEntityIDValid(L, iEntityIndex, 1)) return 0;
	if (iCollectState < 0)
	{
		bRemoveEntityFromGame = true;
	}
	if(bRemoveEntityFromGame==false)
	{
		int iSlotIndex = -1;
		const char* pSpecifiedContainer = "";
		if (n >= 3) iSlotIndex = lua_tonumber(L, 3);
		if (n >= 4) pSpecifiedContainer = lua_tostring(L, 4);
		int iOptionalCollectionID = -1;
		if (n >= 5) iOptionalCollectionID = lua_tonumber(L, 5);
		int iQty = 1;
		if (n >= 6) iQty = lua_tonumber(L, 6);
		for (int containerindex = 0; containerindex < t.inventoryContainers.size(); containerindex++)
		{
			if (iCollectState > 0)
			{
				bool bAllowInHere = false;
				if (iCollectState == 1 && containerindex == 0) bAllowInHere = true; // main inventrory
				if (iCollectState == 2 && containerindex == 1) bAllowInHere = true; // hotkeys inventory
				if (iCollectState == 3 && stricmp(pSpecifiedContainer, t.inventoryContainers[containerindex].Get()) == NULL) bAllowInHere = true; // specified container
				if (bAllowInHere == true && bItemHandled == false)
				{
					// if not already collected
					int n = 0;
					if (iEntityIndex > 0 || bForceMode == false)
					{
						for (n = 0; n < t.inventoryContainer[containerindex].size(); n++)
							if (t.inventoryContainer[containerindex][n].e == iEntityIndex)
								break;
					}
					else
					{
						// forcing to this slot if iEntityIndex zero and forcing in (populating inventory on game load)
						n = t.inventoryContainer[containerindex].size();
					}

					// hotkeys only permits one of each type (so duplicate weapons are deflected to main inv)
					if (iCollectState == 2 && iEntityIndex > 0)
					{
						int entid = t.entityelement[iEntityIndex].bankindex;
						if (t.entityprofile[entid].isweapon > 0)
						{
							int itemCollectionID = find_rpg_collectionindex(t.entityelement[iEntityIndex].eleprof.name_s.Get());
							for (n = 0; n < t.inventoryContainer[containerindex].size(); n++)
							{
								int ee = t.inventoryContainer[containerindex][n].e;
								if (ee != iEntityIndex)
								{
									int eeCollectionID = find_rpg_collectionindex(t.entityelement[ee].eleprof.name_s.Get());
									if (itemCollectionID == eeCollectionID)
									{
										// already one here, so do NOT add
										break;
									}
								}
							}
						}
					}

					if (n >= t.inventoryContainer[containerindex].size())
					{
						// add item to inventory
						inventoryContainerType item;
						item.e = iEntityIndex;
						item.collectionID = -1;

						// find collection ID by matching object name with collection name (cannot use index as user may add to list!)
						if (iEntityIndex > 0)
						{
							int entid = t.entityelement[iEntityIndex].bankindex;
							if (t.entityprofile[entid].isweapon > 0)
							{
								// is a weapon (that are auto added) must use proper internal name for correct identification
								item.collectionID = find_rpg_collectionindex(t.entityprofile[entid].isweapon_s.Get());
								if (item.collectionID == 0)
								{
									// fallback uses regular visible name (ooften used by UI renamed stock weapons)
									item.collectionID = find_rpg_collectionindex(t.entityelement[iEntityIndex].eleprof.name_s.Get());
								}
							}
							else
							{
								item.collectionID = find_rpg_collectionindex(t.entityelement[iEntityIndex].eleprof.name_s.Get());
							}
							if (item.collectionID == 0)
							{
								// if not found from entity details, fall back to specified container item passed in
								item.collectionID = -1;
							}
						}
						if(item.collectionID==-1)
						{
							// not a weapon, can use given name
							if (iOptionalCollectionID != -1)
							{
								// or collection ID if passed in
								item.collectionID = iOptionalCollectionID;
							}
						}

						// if resource and no slot specified (collected in game), merge with any existing
						if (iEntityIndex > 0)
						{
							if (t.entityelement[iEntityIndex].eleprof.iscollectable == 2 && iSlotIndex == -1)
							{
								for (int n = 0; n < t.inventoryContainer[containerindex].size(); n++)
								{
									if (t.inventoryContainer[containerindex][n].collectionID == item.collectionID)
									{
										// already have this resource in container
										iSlotIndex = t.inventoryContainer[containerindex][n].slot;
										break;
									}
								}
							}
							// if resource can never be less than one
							if (t.entityelement[iEntityIndex].eleprof.iscollectable == 2)
							{
								int iQtyToCheck = t.entityelement[iEntityIndex].eleprof.quantity;
								if (bForceMode == true ) iQtyToCheck  = iQty;
								if (iQtyToCheck < 1) iQtyToCheck = 1;
								t.entityelement[iEntityIndex].eleprof.quantity = iQtyToCheck;
							}
						}

						// manage slot for this item
						item.slot = -1;
						if (iSlotIndex == -1)
						{
							// look for free slot index
							iReturnSlot = 0;
							for (int n = 0; n < t.inventoryContainer[containerindex].size(); n++)
							{
								if (t.inventoryContainer[containerindex][n].slot == iReturnSlot)
								{
									// being used - try if next slot free
									iReturnSlot++;
									n = -1;
								}
							}
							item.slot = iReturnSlot;
						}
						else
						{
							// ensure slot not used
							bool bCanUseSlot = true;
							for (int n = 0; n < t.inventoryContainer[containerindex].size(); n++)
							{
								if (t.inventoryContainer[containerindex][n].slot == iSlotIndex)
								{
									if (iEntityIndex > 0)
									{
										if (t.entityelement[iEntityIndex].eleprof.iscollectable == 2)
										{
											// being used by resource - can merge these
											int existingee = t.inventoryContainer[containerindex][n].e;
											if (existingee > 0)
											{
												if (t.entityelement[existingee].eleprof.iscollectable == 2)
												{
													// merge both objects into the present one in the container
													int iQtyToAdd = t.entityelement[iEntityIndex].eleprof.quantity;
													if (iQtyToAdd < 1) iQtyToAdd = 1;
													t.entityelement[existingee].eleprof.quantity += iQtyToAdd;

													// hide other object until need again
													t.entityelement[iEntityIndex].eleprof.quantity = 0;
													bRemoveEntityFromGame = true;
												}
											}
										}
									}

									// being used by non resource - cannot overwrite this one
									bCanUseSlot = false;
									break;
								}
							}
							if (bCanUseSlot == true)
							{
								item.slot = iSlotIndex;
								iReturnSlot = iSlotIndex;
							}
						}

						// finally add to list
						if (iReturnSlot != -1)
						{
							t.inventoryContainer[containerindex].push_back(item);
							bRemoveEntityFromGame = true;
						}

						// handle entity itself
						if (bRemoveEntityFromGame == true) bItemHandled = true;
					}
					else
					{
						// this entity was set for collection, but already in inventory, but still 
						// needs to be removed from the game
						bRemoveEntityFromGame = true;
						bItemHandled = true;
					}
				}
			}
			else
			{
				// find and remove from inventory
				if (bItemHandled == false)
				{
					for (int n = 0; n < t.inventoryContainer[containerindex].size(); n++)
					{
						if (t.inventoryContainer[containerindex][n].e == iEntityIndex)
						{
							t.inventoryContainer[containerindex].erase(t.inventoryContainer[containerindex].begin() + n);
							bItemHandled = true;
							break;
						}
					}
					// when NOT collected, put back in real world
					if (iEntityIndex > 0 && bForceMode == false)
					{
						if (t.entityelement[iEntityIndex].collected != 0)
						{
							t.entityelement[iEntityIndex].collected = 0;
							extern void darklua_refreshhaskeystatefor(LPSTR);
							darklua_refreshhaskeystatefor(t.entityelement[iEntityIndex].eleprof.name_s.Get());
							t.entityelement[iEntityIndex].x += 999999;
							t.entityelement[iEntityIndex].y += 999999;
							t.entityelement[iEntityIndex].z += 999999;
							t.entityelement[iEntityIndex].eleprof.phyalways = 1;
							PositionObject(t.entityelement[iEntityIndex].obj, t.entityelement[iEntityIndex].x, t.entityelement[iEntityIndex].y, t.entityelement[iEntityIndex].z);
							int store = t.e; t.e = iEntityIndex;
							entity_lua_collisionon();
							t.e = store;
						}
					}
				}
			}
		}
	}
	if (bRemoveEntityFromGame == true)
	{
		if (iEntityIndex > 0 && bForceMode == false)
		{
			if (t.entityelement[iEntityIndex].collected == 0)
			{
				int store = t.e; t.e = iEntityIndex;
				entity_lua_collisionoff();
				t.e = store;
				t.entityelement[iEntityIndex].collected = (int)fabs(iCollectState);
				extern void darklua_refreshhaskeystatefor(LPSTR);
				darklua_refreshhaskeystatefor(t.entityelement[iEntityIndex].eleprof.name_s.Get());
				t.entityelement[iEntityIndex].x -= 999999;
				t.entityelement[iEntityIndex].y -= 999999;
				t.entityelement[iEntityIndex].z -= 999999;
				t.entityelement[iEntityIndex].eleprof.phyalways = 1;
				PositionObject(t.entityelement[iEntityIndex].obj, t.entityelement[iEntityIndex].x, t.entityelement[iEntityIndex].y, t.entityelement[iEntityIndex].z);
			}
		}
	}
	lua_pushinteger(L, iReturnSlot);
	return 1;
 }
 int SetEntityCollected(lua_State* L)
 {
	 return SetEntityCollectedEx(L, false);
 }
 int SetEntityCollectedForce(lua_State* L)
 {
	 return SetEntityCollectedEx(L, true);
 }
 int SetEntityUsed(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 int iUsedState = lua_tonumber(L, 2);
		 if (iUsedState < 0 && t.entityelement[iEntityIndex].eleprof.iscollectable == 2)
		 {
			 // resources can be depleted when entity used is, er, used.
			 int qty = t.entityelement[iEntityIndex].eleprof.quantity - 1;
			 if (qty > 0)
			 {
				 iUsedState = 0;
			 }
			 t.entityelement[iEntityIndex].eleprof.quantity = qty;
		 }
		 t.entityelement[iEntityIndex].consumed = iUsedState;
	 }
	 return 0;
 }

 int SetEntityExplodable(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 int iValue = lua_tonumber(L, 2);
		 t.entityelement[iEntityIndex].eleprof.explodable = iValue;
	 }
	 return 0;
 }
 int SetExplosionDamage(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 int iValue = lua_tonumber(L, 2);
		 t.entityelement[iEntityIndex].eleprof.explodedamage = iValue;
	 }
	 return 0;
 }
 int SetExplosionHeight(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 int iValue = lua_tonumber(L, 2);
		 t.entityelement[iEntityIndex].eleprof.explodeheight = iValue;
	 }
	 return 0;
 }
 //PE: SetCustomExplosion(e,effectname) sound is using <Sound5>. SetCustomExplosionShould only be called one time.
 int SetCustomExplosion(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 const char* pEffect = lua_tostring(L, 2);
		 if (!pEffect)
			 return 0;
		 t.entityelement[iEntityIndex].eleprof.explodable_decalname = pEffect;
		 //PE: Init effects.
		 if (t.entityelement[iEntityIndex].soundset6 > 0) deleteinternalsound(t.entityelement[iEntityIndex].soundset6);
		 t.entityelement[iEntityIndex].soundset6 = loadinternalsoundcore(t.entityelement[iEntityIndex].eleprof.soundset6_s.Get(), 1);
		
		 if (t.entityelement[iEntityIndex].eleprof.explodable_decalname.Len() > 0)
		 {
			 cstr explodename = t.entityelement[iEntityIndex].eleprof.explodable_decalname;
			 cstr lexplodename = explodename.Lower();

			 if (lexplodename.Len() > 0)
			 {
				 for (int i = 1; i <= g.decalmax; i++)
				 {
					 cstr ldecal = t.decal[i].name_s.Lower();
					 if (ldecal == lexplodename)
					 {
						 int alreadyactive = t.decal[i].active;
						 t.decal[i].active = 1;
						 t.decal[i].newparticle.iMaxCache = 2; //PE: For now only 2 cached custom explosions.
						 if (alreadyactive != 1)
						 {
							 //PE: Never change t. variables in lua.
							 cstr oldtdecal_s = t.decal_s;
							 int oldtdecalid = t.decalid;
							 t.decal_s = t.decal[i].name_s;
							 t.decalid = i;
							 decal_load();
							 t.decalid = oldtdecalid;
							 t.decal_s = oldtdecal_s;
						 }
						 break;
					 }
				 }
			 }
		 }
	 }
	 return 0;
 }


 int GetEntityExplodable(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int e = lua_tonumber(L, 1);
	 int iReturnValue = 0;
	 if (e > 0 && e < t.entityelement.size())
	 {
		 iReturnValue = t.entityelement[e].eleprof.explodable;
	 }
	 lua_pushinteger(L, iReturnValue);
	 return 1;

 }

 int GetEntityObjective(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int e = lua_tonumber(L, 1);
	 int iReturnValue = 0;
	 if (e > 0 && e < t.entityelement.size())
	 {
		 iReturnValue = t.entityelement[e].eleprof.isobjective;
		 if (t.entityelement[e].eleprof.isobjective_alwaysactive)
			 iReturnValue = 3;
	 }
	 lua_pushinteger(L, iReturnValue);
	 return 1;
 }

 int GetEntityProjectGlobal(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int e = lua_tonumber(L, 1);
	 int iReturnValue = 0;
	 if (e > 0 && e < t.entityelement.size() )
	 {
		 iReturnValue = t.entityelement[e].eleprof.isProjectGlobal;
	 }
	 lua_pushinteger(L, iReturnValue);
	 return 1;
 }

 int GetEntityCollectable(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int e = lua_tonumber(L, 1);
	 int iReturnValue = 0;
	 if (e > 0 && e < t.entityelement.size())
	 {
		 iReturnValue = t.entityelement[e].eleprof.iscollectable;
	 }
	 lua_pushinteger(L, iReturnValue);
	 return 1;
 }
 int GetEntityCollected(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int e = lua_tonumber(L, 1);
	 int iReturnValue = 0;
	 if (e > 0 && e < t.entityelement.size())
	 {
		 iReturnValue = t.entityelement[e].collected;
	 }
	 lua_pushinteger(L, iReturnValue);
	 return 1;
 }
 int GetEntityUsed(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int e = lua_tonumber(L, 1);
	 int iReturnValue = 0;
	 if (e > 0 && e < t.entityelement.size())
	 {
		 iReturnValue = t.entityelement[e].consumed;
	 }
	 lua_pushinteger(L, iReturnValue);
	 return 1;
 }
 int SetEntityQuantity(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 int iQty = lua_tonumber(L, 2);
		 t.entityelement[iEntityIndex].eleprof.quantity = iQty;
	 }
	 return 0;
 }
 int GetEntityQuantity(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int e = lua_tonumber(L, 1);
	 int iQty = 0;
	 if (e > 0 && e < t.entityelement.size())
	 {
		 iQty = t.entityelement[e].eleprof.quantity;
	 }
	 lua_pushinteger(L, iQty);
	 return 1;
 }

 int GetEntityWhoActivated(lua_State *L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int e = lua_tonumber(L, 1);
	 int iReturnValue = 0;
	 if (e > 0 && e < t.entityelement.size())
	 {
		 iReturnValue = t.entityelement[e].whoactivated;
	 }
	 lua_pushinteger (L, iReturnValue);
	 return 1;
 }
 int GetEntityActive(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iIndex = lua_tonumber(L, 1);
	int iReturnValue = 0;
	if ( LuaEntityIDValid ( L, iIndex, 1 ) )
	{
		iReturnValue = t.entityelement[iIndex].active;
		if ( Len(t.entityelement[iIndex].eleprof.aimainname_s.Get())>1 ) 
			if ( t.entityelement[iIndex].eleprof.aimain == 0 ) 
				iReturnValue = 0;
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetEntityVisibility(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iObjectNumber = t.entityelement[iEntityIndex].obj;
		if ( iObjectNumber > 0 )
		{
			sObject* pObject = GetObjectData ( iObjectNumber );
			if ( pObject && pObject->bVisible == true )
				iReturnValue = 1;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int SetEntitySpawnAtStart(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if (LuaEntityIDValid(L, iEntityIndex, 1))
	{
		t.entityelement[iEntityIndex].eleprof.spawnatstart = lua_tonumber(L, 2);
	}
	return 0;
 }
 int GetEntitySpawnAtStart(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if (LuaEntityIDValid(L, iEntityIndex, 1))
	{
		iReturnValue = t.entityelement[iEntityIndex].eleprof.spawnatstart;
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetEntityFilePath(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	char pReturnValue[1024];
	strcpy ( pReturnValue, "" );
	int iEntityIndex = lua_tonumber(L, 1);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iEntID = t.entityelement[iEntityIndex].bankindex;
		if ( iEntID > 0 )
		{
			strcpy ( pReturnValue, t.entitybank_s[iEntID].Get() );
		}
	}
	lua_pushstring ( L, pReturnValue );
	return 1;
 }

 int GetEntityClonedSinceStartValue(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iReturnValue = 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (LuaEntityIDValid(L, iEntityIndex, 1)) iReturnValue = t.entityelement[iEntityIndex].iWasSpawnedInGame;
	 lua_pushinteger (L, iReturnValue);
	 return 1;
 }

 int SetPreExitValue(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if (LuaEntityIDValid(L, iEntityIndex, 1))
	{
		t.entityelement[iEntityIndex].eleprof.aipreexit = lua_tonumber(L, 2);
	}
	return 0;
 }

 int RawSetEntityData ( lua_State *L, int iDataMode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if (iEntityIndex > 0 && iEntityIndex < t.entityelement.size())
	{
		float fValue = lua_tonumber(L, 2);
		switch (iDataMode)
		{
			case 12: // 120417 - change anim speed mod, and character speed if a character
			{
				if (t.entityelement[iEntityIndex].animspeedmod != fValue)
				{
					t.entityelement[iEntityIndex].animspeedmod = fValue;
					float fFinalAnimSpeed = t.entityelement[iEntityIndex].eleprof.animspeed * t.entityelement[iEntityIndex].animspeedmod;
					t.e = iEntityIndex;
					entity_lua_findcharanimstate ();
					if (t.tcharanimindex != -1) t.charanimstates[t.tcharanimindex].animationspeed_f = (65.0f / 100.0f) * fFinalAnimSpeed;
				}
				break;
			}
			case 22: t.entityelement[iEntityIndex].eleprof.phyalways = (int)fValue; break;
			case 23: t.entityelement[iEntityIndex].iCanGoUnderwater = (int)fValue; break;

			case 104: t.entityelement[iEntityIndex].eleprof.conerange = fValue; break;

			case 105:
			{
				t.entityelement[iEntityIndex].eleprof.iMoveSpeed = (int)fValue;
				entity_lua_findcharanimstate();
				if (t.tcharanimindex != -1)
				{
					int movingbackward = 0; if (fValue < 0.0f) movingbackward = 1;
					t.charanimstate.movingbackward = movingbackward;
					t.charanimstate.movespeed_f = fabs(fValue) / 100.0f;
					t.charanimstates[t.tcharanimindex] = t.charanimstate;
				}
				break;
			}
			case 106:
			{
				t.entityelement[iEntityIndex].eleprof.iTurnSpeed = (int)fValue;
				entity_lua_findcharanimstate();
				if (t.tcharanimindex != -1)
				{
					t.charanimstate.turnspeed_f = fValue / 100.0f;
					t.charanimstates[t.tcharanimindex] = t.charanimstate;
				}
				break;
			}
		}
	}
	return 0;
 }

 int GetEntityData ( lua_State *L, int iDataMode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( iDataMode == 19 )
	{
		if ( n < 4 ) return 0;
	}
	else
	{
		if ( n < 1 ) return 0;
	}
	int iEntityIndex = lua_tonumber(L, 1);
	if ( iEntityIndex > 0 && iEntityIndex < t.entityelement.size() )
	{
		if ( iDataMode < 101 )
		{
			float fReturnValue = 0;
			int iObjectNumber = t.entityelement[iEntityIndex].obj;
			if ( iObjectNumber > 0 )
			{
				sObject* pObject = GetObjectData ( iObjectNumber );
				if ( pObject )
				{
					switch ( iDataMode )
					{
						case 1 : fReturnValue = pObject->position.vecPosition.x; break;
						case 2 : fReturnValue = pObject->position.vecPosition.y; break;
						case 3 : fReturnValue = pObject->position.vecPosition.z; break;
						case 4 : fReturnValue = pObject->position.vecRotate.x; break;
						case 5 : fReturnValue = pObject->position.vecRotate.y; break;
						case 6 : fReturnValue = pObject->position.vecRotate.z; break;
						case 11 : fReturnValue = t.entityelement[iEntityIndex].eleprof.animspeed/100.0f; break;
						case 12 : fReturnValue = t.entityelement[iEntityIndex].animspeedmod; break;
						case 13 : 
						{
							// work out delta from last position and current position
							float fDX = t.entityelement[iEntityIndex].x - t.entityelement[iEntityIndex].lastx;
							float fDY = t.entityelement[iEntityIndex].y - t.entityelement[iEntityIndex].lasty;
							float fDZ = t.entityelement[iEntityIndex].z - t.entityelement[iEntityIndex].lastz;
							fReturnValue = sqrt ( fabs(fDX*fDX)+fabs(fDY*fDY)+fabs(fDZ*fDZ) );
							break;
						}
						case 14 : 
						{
							// Return collision box coordinates (6 values )
							lua_pushnumber( L, pObject->collision.vecMin.x );
							lua_pushnumber( L, pObject->collision.vecMin.y );
							lua_pushnumber( L, pObject->collision.vecMin.z );
							lua_pushnumber( L, pObject->collision.vecMax.x );
							lua_pushnumber( L, pObject->collision.vecMax.y );
							lua_pushnumber( L, pObject->collision.vecMax.z );
							return 6;
						}
						case 15 :
						{
							// Position and Angle together (6 values )
							lua_pushnumber( L, pObject->position.vecPosition.x );
							lua_pushnumber( L, pObject->position.vecPosition.y );
							lua_pushnumber( L, pObject->position.vecPosition.z );
							lua_pushnumber( L, pObject->position.vecRotate.x );
							lua_pushnumber( L, pObject->position.vecRotate.y );
							lua_pushnumber( L, pObject->position.vecRotate.z );
							return 6;
						}
						case 16 : fReturnValue = t.entityelement[iEntityIndex].eleprof.phyweight; break;
						case 17 : 
						{
							// Scale factors 3 values
							lua_pushnumber( L, pObject->position.vecScale.x );
							lua_pushnumber( L, pObject->position.vecScale.y );
							lua_pushnumber( L, pObject->position.vecScale.z );
							return 3;
						}
						case 18 : 
						{
							lua_pushstring(L, t.entityelement[iEntityIndex].eleprof.name_s.Get() );
							return 1;
						}
						case 19 : 
						{
							// as above, but done manually with no outside assistance from neighboring systems
							float fThisPosX = lua_tonumber(L, 2);
							float fThisPosY = lua_tonumber(L, 3);
							float fThisPosZ = lua_tonumber(L, 4);
							float fDX = fThisPosX - t.entityelement[iEntityIndex].customlastx;
							float fDY = fThisPosY - t.entityelement[iEntityIndex].customlasty;
							float fDZ = fThisPosZ - t.entityelement[iEntityIndex].customlastz;
							t.entityelement[iEntityIndex].customlastx = fThisPosX;
							t.entityelement[iEntityIndex].customlasty = fThisPosY;
							t.entityelement[iEntityIndex].customlastz = fThisPosZ;
							fReturnValue = sqrt ( fabs(fDX*fDX)+fabs(fDY*fDY)+fabs(fDZ*fDZ) );
							break;
						}
						case 20: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.speed / 100.0f; break;
						case 21: 
						{
							int entid = t.entityelement[iEntityIndex].bankindex;
							fReturnValue = (float)t.entityprofile[entid].ismarker;
							break;
						}
						case 22: fReturnValue = t.entityelement[iEntityIndex].eleprof.phyalways; break;
						case 23: fReturnValue = t.entityelement[iEntityIndex].iCanGoUnderwater; break;
						case 24: fReturnValue = t.entityelement[iEntityIndex].bankindex; break;
					}
				}
			}
			lua_pushnumber ( L, fReturnValue );
		}
		else
		{
			int iReturnValue = 0;
			switch ( iDataMode )
			{
				case 101: 
				{
					#ifdef WICKEDENGINE
					t.e = iEntityIndex;
					entity_lua_findcharanimstate();
					if (t.tcharanimindex != -1)
					{
						extern int darkai_canshoot (void);
						iReturnValue = darkai_canshoot();
						t.charanimstates[t.tcharanimindex] = t.charanimstate;
					}
					#endif
					break;
				}
				case 102:
				{
					#ifdef WICKEDENGINE
					iReturnValue = t.entityelement[iEntityIndex].eleprof.hasweapon;
					#endif
					break;
				}
				case 103:
				{
					#ifdef WICKEDENGINE
					iReturnValue = t.entityelement[iEntityIndex].eleprof.coneangle;
					#endif
					break;
				}
				case 104:
				{
					#ifdef WICKEDENGINE
					iReturnValue = t.entityelement[iEntityIndex].eleprof.conerange;
					#endif
					break;
				}
				case 105:
				{
					#ifdef WICKEDENGINE
					iReturnValue = t.entityelement[iEntityIndex].eleprof.iMoveSpeed;
					#endif
					break;
				}
				case 106:
				{
					#ifdef WICKEDENGINE
					iReturnValue = t.entityelement[iEntityIndex].eleprof.iTurnSpeed;			
					#endif
					break;
				}
			}
			lua_pushnumber ( L, iReturnValue );
		}
	}
	else
	{
		lua_pushnumber ( L, 0 );
	}
	return 1;
 }

 int GetEntityPositionX(lua_State *L) { return GetEntityData ( L, 1 ); }
 int GetEntityPositionY(lua_State *L) { return GetEntityData ( L, 2 ); }
 int GetEntityPositionZ(lua_State *L) { return GetEntityData ( L, 3 ); }
 int GetEntityAngleX(lua_State *L) { return GetEntityData ( L, 4 ); }
 int GetEntityAngleY(lua_State *L) { return GetEntityData ( L, 5 ); }
 int GetEntityAngleZ(lua_State *L) { return GetEntityData ( L, 6 ); }
 int GetMovementSpeed(lua_State *L) { return GetEntityData(L, 20); }
 int GetAnimationSpeed(lua_State *L) { return GetEntityData ( L, 11 ); }
 int SetAnimationSpeedModulation(lua_State *L) { return RawSetEntityData ( L, 12 ); }
 int GetAnimationSpeedModulation(lua_State *L) { return GetEntityData ( L, 12 ); }
 int GetMovementDelta(lua_State *L) { return GetEntityData ( L, 13 ); }
 int GetEntityCollBox(lua_State* L) { return GetEntityData (L, 14); }
 int GetEntityColBox(lua_State* L) { return GetEntityData (L, 14); }
 int GetEntityPosAng(lua_State *L)  { return GetEntityData ( L, 15 ); }
 int GetEntityWeight(lua_State *L)  { return GetEntityData ( L, 16 ); }
 int GetEntityScales(lua_State *L)  { return GetEntityData ( L, 17 ); }
 int GetEntityName(lua_State *L)    { return GetEntityData ( L, 18 ); }
 int GetMovementDeltaManually(lua_State *L) { return GetEntityData ( L, 19 ); }
 int GetEntityMarkerMode(lua_State *L) { return GetEntityData (L, 21); }

 int SetEntityAlwaysActive(lua_State* L) { return RawSetEntityData (L, 22); }
 int GetEntityAlwaysActive(lua_State* L) { return GetEntityData (L, 22); }

 int SetEntityUnderwaterMode(lua_State* L) { return RawSetEntityData (L, 23); }
 int GetEntityUnderwaterMode(lua_State* L) { return GetEntityData (L, 23); }

 int GetEntityParentID(lua_State* L) { return GetEntityData (L, 24); }

 int SetEntityIfUsed(lua_State* L) 
 { 
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 const char* pString = lua_tostring(L, 2);
	 if (!LuaEntityIDValid(L, iEntityIndex, 1)) return 0;
	 t.entityelement[iEntityIndex].eleprof.ifused_s = pString;
	 return 0;
 }
 int GetEntityIfUsed(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (!LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 lua_pushstring(L, "");
		 return 1;
	 }
	 lua_pushstring(L, t.entityelement[iEntityIndex].eleprof.ifused_s.Get());
	 return 1;
 }

 #ifdef WICKEDENGINE
 int GetEntityCanFire(lua_State *L) { return GetEntityData (L, 101); }
 int GetEntityHasWeapon(lua_State *L) { return GetEntityData (L, 102); }
 int GetEntityViewAngle(lua_State *L) { return GetEntityData (L, 103); }
 int GetEntityViewRange(lua_State *L) { return GetEntityData (L, 104); }
 int SetEntityViewRange(lua_State *L) { return RawSetEntityData (L, 104); }
 
 int GetEntityMoveSpeed(lua_State *L) { return GetEntityData (L, 105); }
 int GetEntityTurnSpeed(lua_State *L) { return GetEntityData (L, 106); }
 int SetEntityMoveSpeed(lua_State *L) { return RawSetEntityData (L, 105); }
 int SetEntityTurnSpeed(lua_State *L) { return RawSetEntityData (L, 106); }
#endif

 #ifdef WICKEDENGINE
 int SetEntityRelationshipData (lua_State *L, int iDataMode)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iNewValue = lua_tonumber(L, 2);
	if (!LuaEntityIDValid(L, iEntityIndex, 1)) return 0;
	switch (iDataMode)
	{
		case 1: 
		{
			if (t.entityprofile[t.entityelement[iEntityIndex].bankindex].ischaracter == 1)
			{
				t.entityelement[iEntityIndex].eleprof.iCharAlliance = iNewValue;
			}
		}
		break;
	}
	return 0;
 }
 int GetEntityRelationshipData (lua_State *L, int iDataMode)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1 || n > 2) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iSubscriptValue = lua_tonumber(L, 2);
	if (LuaEntityIDValid(L, iEntityIndex, 1))
	{
		float fReturnValue = 0;
		int iObjectNumber = t.entityelement[iEntityIndex].obj;
		if (iObjectNumber > 0)
		{
			sObject* pObject = GetObjectData (iObjectNumber);
			if (pObject)
			{
				switch (iDataMode)
				{
					case 1: 
					{
						if (t.entityprofile[t.entityelement[iEntityIndex].bankindex].ischaracter == 1)
							fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.iCharAlliance;
						else
							fReturnValue = -1;
						break;
					}
					case 2: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.iCharFaction; break;
					case 3: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.iObjectReserved1; break;
					case 4: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.iObjectReserved2; break;
					case 5: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.iObjectReserved3; break;
					case 6: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.iCharPatrolMode; break;
					case 7: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.fCharRange[0]; break;
					case 8: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.fCharRange[1]; break;
					case 11: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.fObjectDataReserved[iSubscriptValue]; break;
					case 12:
					{
						int iLinkID = t.entityelement[iEntityIndex].eleprof.iObjectRelationships[iSubscriptValue];
						if (iLinkID)
						{
							// find real entityindex associated with link id (links can be forged but entities not exist - editor error can cause a crash!!)
							for (int ee = 1; ee <= g.entityelementmax; ee++)
							{
								int entid = t.entityelement[ee].bankindex;
								if (entid > 0)
								{
									if (t.entityelement[ee].active != 0 && t.entityelement[ee].staticflag == 0 )
									{
										if (t.entityelement[ee].eleprof.iObjectLinkID == iLinkID)
										{
											fReturnValue = (float)ee;
											break;
										}
									}
								}
							}
						}
						else
						{
							fReturnValue = 0;
						}
						break;
					}
					case 13: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.iObjectRelationshipsType[iSubscriptValue]; break;
					case 14: fReturnValue = (float)t.entityelement[iEntityIndex].eleprof.iObjectRelationshipsData[iSubscriptValue]; break;
				}
			}
		}
		lua_pushnumber (L, fReturnValue);
	}
	else
	{
		lua_pushnumber (L, 0);
	}
	return 1;
 }
 int SetEntityAllegiance(lua_State *L) { return SetEntityRelationshipData (L, 1); }
 int GetEntityAllegiance(lua_State *L) { return GetEntityRelationshipData (L, 1); }
 int GetEntityPatrolMode(lua_State *L) { return GetEntityRelationshipData (L, 6); }
 int GetEntityRelationshipID(lua_State *L) { return GetEntityRelationshipData (L, 12); }
 int GetEntityRelationshipType(lua_State *L) { return GetEntityRelationshipData (L, 13); }
 int GetEntityRelationshipData(lua_State *L) { return GetEntityRelationshipData (L, 14); }
#endif

 #ifdef WICKEDENGINE
 std::vector<int> g_entitiesinconelist;
 int GetEntitiesWithinCone(lua_State *L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 8) return 0;
	 float fX = lua_tonumber(L, 1);
	 float fY = lua_tonumber(L, 2);
	 float fZ = lua_tonumber(L, 3);
	 float fAX = 0;// lua_tonumber(L, 4);
	 float fAY = lua_tonumber(L, 5);
	 float fAZ = 0;// lua_tonumber(L, 6);
	 float fConeAngle = lua_tonumber(L, 7);
	 float fConeDistance = lua_tonumber(L, 8);
	 // scan all entities and create list of ones inside cone
	 g_entitiesinconelist.clear();
	 for ( int e = 1; e <= g.entityelementlist; e++)
	 {
		 if (t.entityelement[e].bankindex > 0 && t.entityelement[e].health > 0 )
		 {
			 float fDX = t.entityelement[e].x - fX;
			 float fDY = t.entityelement[e].y - fY;
			 float fDZ = t.entityelement[e].z - fZ;
			 float fDD = sqrt(fabs(fDX*fDX) + fabs(fDY*fDY) + fabs(fDZ*fDZ));
			 if (fDD > 0.0f && fDD < fConeDistance)
			 {
				 float fDA = fAY - atan2deg(fDX, fDZ);
				 if (fDA < -180) fDA += 360.0f;
				 if (fDA > 180) fDA -= 360.0f;
				 if (fabs(fDA) < fConeAngle)
				 {
					 g_entitiesinconelist.push_back(e);
				 }
			 }
		 }
	 }
	 int iCount = g_entitiesinconelist.size();
	 lua_pushnumber (L, iCount);
	 return 1;
 }
 int GetEntityWithinCone(lua_State *L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iIndex = lua_tonumber(L, 1);
	 int iEntityIndex = 0;
	 if (iIndex >= 0 && iIndex < g_entitiesinconelist.size())
	 {
		 iEntityIndex = g_entitiesinconelist[iIndex];
	 }
	 // return entity e from list at this index
	 lua_pushnumber (L, iEntityIndex);
	 return 1;
 }
 #endif

 #ifdef WICKEDENGINE
 extern std::vector<int> g_iDestroyedEntitiesList;
 int GetNearestEntityDestroyed(lua_State* L)
 {
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iMode = lua_tonumber(L, 1);
	 int iBestE = 0;
	 if (g_iDestroyedEntitiesList.size() > 0)
	 {
		 int entrytoremove = -1;
		 float fBestDist = 999999.9f;
		 for (int n = 0; n < g_iDestroyedEntitiesList.size(); n++)
		 {
			 int e = g_iDestroyedEntitiesList[n];
			 float fX = t.entityelement[e].x;
			 float fY = t.entityelement[e].y;
			 float fZ = t.entityelement[e].z;
			 float fPlrX = ObjectPositionX(t.aisystem.objectstartindex);
			 float fPlrY = ObjectPositionY(t.aisystem.objectstartindex);
			 float fPlrZ = ObjectPositionZ(t.aisystem.objectstartindex);
			 float fDX = fPlrX - fPlrX;
			 float fDY = fPlrY - fPlrY;
			 float fDZ = fPlrZ - fPlrZ;
			 float fDist = sqrt(fabs(fDX* fDX) + fabs(fDY* fDY) + fabs(fDZ* fDZ));
			 if (fDist < fBestDist)
			 {
				 entrytoremove = n;
				 fBestDist = fDist;
				 iBestE = e;
			 }
		 }
		 if (entrytoremove > -1)
		 {
			 g_iDestroyedEntitiesList.erase(g_iDestroyedEntitiesList.begin() + entrytoremove);
			 entrytoremove = -1;
		 }
	 }
	 lua_pushnumber (L, iBestE);
	 return 1;
 }
 int GetNearestSoundDistance(lua_State *L)
 {
	 int n = LUA_GETTOP(L);
	 if (n < 4) return 0;
	 float fX = lua_tonumber(L, 1);
	 float fY = lua_tonumber(L, 2);
	 float fZ = lua_tonumber(L, 3);
	 int iCategory = lua_tonumber(L, 4);
	 int iWhoE = 0;
	 float getClosestSoundWithinRange (float fX, float fY, float fZ, int iCategory, int* iWhoE);
	 float fDistance = getClosestSoundWithinRange (fX, fY, fZ, iCategory, &iWhoE);
	 lua_pushnumber (L, fDistance);
	 lua_pushnumber (L, iWhoE);
	 return 2;
 }
 int MakeAISound (lua_State *L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 6) return 0;
	 t.tsx_f = lua_tonumber(L, 1);
	 t.tsy_f = lua_tonumber(L, 2);
	 t.tsz_f = lua_tonumber(L, 3);
	 t.tradius_f = lua_tonumber(L, 4);
	 int iCategory = lua_tonumber(L, 5);
	 int iWhoE = lua_tonumber(L, 6);
	 void darkai_makesound_ex (int iCategory, int iWhoE);
	 darkai_makesound_ex (iCategory, iWhoE);
	 return 0;
 }
 #endif

 #ifdef WICKEDENGINE
 int GetTerrainEditableArea(lua_State *L)
 {
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iDimension = lua_tonumber(L, 1);
	 float fTerrainEditableAreaSize = 0.0f;
	 switch (iDimension)
	 {
		 case 0: fTerrainEditableAreaSize = t.terraineditableareasizeminx; break;
		 case 1: fTerrainEditableAreaSize = t.terraineditableareasizeminz; break;
		 case 2: fTerrainEditableAreaSize = t.terraineditableareasizemaxx; break;
		 case 3: fTerrainEditableAreaSize = t.terraineditableareasizemaxz; break;
	 }
	 lua_pushnumber (L, fTerrainEditableAreaSize);
	 return 1;
 }
 #endif

 int SetEntityString(lua_State *L)
 {
	lua = L;
	int n = lua_gettop( L );
	if ( n < 3 ) return 0;
	bool setSound = ( n == 4 ) && lua_tonumber( L, 4 ) == 1;
	int iReturnValue = 0;
	int iEntityIndex    = lua_tonumber( L, 1 );
	int iSlotIndex      = lua_tonumber( L, 2 );
	const char* pString = lua_tostring( L, 3 );
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		if ( iSlotIndex == 0 ) 
		{
			t.entityelement[ iEntityIndex ].eleprof.soundset_s = pString;
			if ( setSound )
			{
				if ( t.entityelement[ iEntityIndex ].soundset > 0 ) deleteinternalsound( t.entityelement[ iEntityIndex ].soundset );
				t.entityelement[ iEntityIndex ].soundset = loadinternalsoundcore( t.entityelement[ iEntityIndex ].eleprof.soundset_s.Get(), 1 );
			}
		}
		if ( iSlotIndex == 1 )
		{
			t.entityelement[ iEntityIndex ].eleprof.soundset1_s = pString;
			if ( setSound )
			{
				if ( t.entityelement[ iEntityIndex ].soundset1 > 0 ) deleteinternalsound( t.entityelement[ iEntityIndex ].soundset1 );
				t.entityelement[ iEntityIndex ].soundset1 =	loadinternalsoundcore( t.entityelement[ iEntityIndex ].eleprof.soundset1_s.Get(), 1 );
			}
		}
		if ( iSlotIndex == 2 ) 
		{ 
			t.entityelement[ iEntityIndex ].eleprof.soundset2_s = pString; 
			if ( setSound )
			{
				if ( t.entityelement[ iEntityIndex ].soundset2 > 0 ) deleteinternalsound( t.entityelement[ iEntityIndex ].soundset2 );
				t.entityelement[ iEntityIndex ].soundset2 =	loadinternalsoundcore( t.entityelement[ iEntityIndex ].eleprof.soundset2_s.Get(), 1 );
			}
		}
		if ( iSlotIndex == 3 ) 
		{ 
			t.entityelement[ iEntityIndex ].eleprof.soundset3_s = pString; 
			if ( setSound )
			{
				if ( t.entityelement[ iEntityIndex ].soundset3 > 0 ) deleteinternalsound( t.entityelement[ iEntityIndex ].soundset3 );
				t.entityelement[ iEntityIndex ].soundset3 =	loadinternalsoundcore( t.entityelement[ iEntityIndex ].eleprof.soundset3_s.Get(), 1 );
			}
		}
		//t.entityelement[iEntityIndex].soundset4 = 0;
		if (iSlotIndex == 4)
		{
			t.entityelement[iEntityIndex].eleprof.soundset4a_s = pString;
			if (setSound)
			{
				if (t.entityelement[iEntityIndex].soundset4 > 0) deleteinternalsound(t.entityelement[iEntityIndex].soundset4);
				t.entityelement[iEntityIndex].soundset4 = loadinternalsoundcore(t.entityelement[iEntityIndex].eleprof.soundset4a_s.Get(), 1);
			}
		}

		if (iSlotIndex == 5)
		{
			t.entityelement[iEntityIndex].eleprof.soundset5_s = pString;
			if (setSound)
			{
				if (t.entityelement[iEntityIndex].soundset5 > 0) deleteinternalsound(t.entityelement[iEntityIndex].soundset5);
				t.entityelement[iEntityIndex].soundset5 = loadinternalsoundcore(t.entityelement[iEntityIndex].eleprof.soundset5_s.Get(), 1);
			}
		}
		if (iSlotIndex == 6)
		{
			t.entityelement[iEntityIndex].eleprof.soundset6_s = pString;
			if (setSound)
			{
				if (t.entityelement[iEntityIndex].soundset6 > 0) deleteinternalsound(t.entityelement[iEntityIndex].soundset6);
				t.entityelement[iEntityIndex].soundset6 = loadinternalsoundcore(t.entityelement[iEntityIndex].eleprof.soundset6_s.Get(), 1);
			}
		}
	}
	return 0;
 }
 int GetEntityString(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iSlotIndex = lua_tonumber(L, 2);
	LPSTR pString = "";
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		if ( iSlotIndex == 0 ) pString = t.entityelement[iEntityIndex].eleprof.soundset_s.Get();
		if ( iSlotIndex == 1 ) pString = t.entityelement[iEntityIndex].eleprof.soundset1_s.Get();
		if ( iSlotIndex == 2 ) pString = t.entityelement[iEntityIndex].eleprof.soundset2_s.Get();
		if ( iSlotIndex == 3 ) pString = t.entityelement[iEntityIndex].eleprof.soundset3_s.Get();
		if ( iSlotIndex == 4 ) pString = t.entityelement[iEntityIndex].eleprof.soundset4a_s.Get();
		if ( iSlotIndex == 5 ) pString = t.entityelement[iEntityIndex].eleprof.soundset5_s.Get();
		if ( iSlotIndex == 6 ) pString = t.entityelement[iEntityIndex].eleprof.soundset6_s.Get();
	}
	lua_pushstring ( L, pString );
	return 1;
 }
 int GetLimbName(lua_State *L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iID = lua_tonumber(L, 1);
	 LPSTR pString = "";
	 if (ObjectExist(iID))
	 {
		 int iLimbNum = lua_tonumber(L, 2);
		 if (iLimbNum < 1 || iLimbNum >= g_ObjectList[iID]->iFrameCount) return 0;

		 // check the object exists
		 if (ConfirmObjectAndLimb(iID, iLimbNum))
		 {
			 // get name of frame
			 sObject* pObject = g_ObjectList[iID];
			 LPSTR pLimbName = pObject->ppFrameList[iLimbNum]->szName;
			 pString = pLimbName;
		 }
		 //LPSTR pString = LimbName(lua_tonumber(L, 1), lua_tonumber(L, 2)); //PE: Leak mem.
	 }
	 lua_pushstring(L, pString);
	 return 1;
 }

 // limbs of an entity's object, numbered as GetLimbName and SetLimbIndex number them; the renderer follows each change
 static sObject* LuaEntityLimbObject ( lua_State* L, int e, int iLimb )
 {
	 if ( !LuaEntityIDValid ( L, e, 1 ) ) return NULL;
	 int iObj = t.entityelement[e].obj;
	 if ( iObj <= 0 || ObjectExist ( iObj ) == 0 ) return NULL;
	 sObject* pObject = GetObjectData ( iObj );
	 if ( !pObject || !pObject->ppFrameList || iLimb < 0 || iLimb >= pObject->iFrameCount || !pObject->ppFrameList[iLimb] ) return NULL;
	 return pObject;
 }
 int GetEntityLimbCount ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 1 ) return 0;
	 int e = lua_tointeger(L, 1);
	 int iCount = 0;
	 if ( LuaEntityIDValid ( L, e, 1 ) )
	 {
		 int iObj = t.entityelement[e].obj;
		 if ( iObj > 0 && ObjectExist ( iObj ) == 1 ) iCount = GetObjectData ( iObj )->iFrameCount;
	 }
	 lua_pushinteger(L, iCount);
	 return 1;
 }
 int GetEntityLimbName ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 lua_pushstring(L, pObject ? pObject->ppFrameList[iLimb]->szName : "");
	 return 1;
 }
 // SetEntityLimbRotation(e, limb, ax, ay, az): degrees, in the limb's own space on top of its pose in the model file
 int SetEntityLimbRotation ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 5 ) return 0;
	 int e = lua_tointeger(L, 1);
	 int iLimb = lua_tointeger(L, 2);
	 sObject* pObject = LuaEntityLimbObject ( L, e, iLimb );
	 if ( !pObject ) return 0;
	 RotateLimb ( t.entityelement[e].obj, iLimb, lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5) );
	 WickedCall_UpdateLimbFromFrame ( pObject, iLimb );
	 return 0;
 }
 int GetEntityLimbRotation ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 GGVECTOR3 vecRotation = pObject ? pObject->ppFrameList[iLimb]->vecRotation : GGVECTOR3(0, 0, 0);
	 lua_pushnumber(L, vecRotation.x);
	 lua_pushnumber(L, vecRotation.y);
	 lua_pushnumber(L, vecRotation.z);
	 return 3;
 }
 // SetEntityLimbOffset(e, limb, x, y, z): moves the limb in its own space, for suspension travel or recoil
 int SetEntityLimbOffset ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 5 ) return 0;
	 int e = lua_tointeger(L, 1);
	 int iLimb = lua_tointeger(L, 2);
	 sObject* pObject = LuaEntityLimbObject ( L, e, iLimb );
	 if ( !pObject ) return 0;
	 OffsetLimb ( t.entityelement[e].obj, iLimb, lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5) );
	 WickedCall_UpdateLimbFromFrame ( pObject, iLimb );
	 return 0;
 }
 int GetEntityLimbOffset ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 GGVECTOR3 vecOffset = pObject ? pObject->ppFrameList[iLimb]->vecOffset : GGVECTOR3(0, 0, 0);
	 lua_pushnumber(L, vecOffset.x);
	 lua_pushnumber(L, vecOffset.y);
	 lua_pushnumber(L, vecOffset.z);
	 return 3;
 }
 // SetEntityLimbPivot(e, limb, x, y, z): the point in the limb's own space that its rotation turns about. 0,0,0 is the limb's
 // origin, which on models whose parts were exported in place is the model's origin; GetEntityLimbBounds gives the limb's
 // extent in the same space, so its centre is a rotor hub or a wheel axle
 int SetEntityLimbPivot ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 5 ) return 0;
	 int e = lua_tointeger(L, 1);
	 int iLimb = lua_tointeger(L, 2);
	 sObject* pObject = LuaEntityLimbObject ( L, e, iLimb );
	 if ( !pObject ) return 0;
	 sFrame* pFrame = pObject->ppFrameList[iLimb];
	 pFrame->vecPivot = GGVECTOR3 ( lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5) );
	 RotateLimb ( t.entityelement[e].obj, iLimb, pFrame->vecRotation.x, pFrame->vecRotation.y, pFrame->vecRotation.z );
	 WickedCall_UpdateLimbFromFrame ( pObject, iLimb );
	 return 0;
 }
 int GetEntityLimbPivot ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 GGVECTOR3 vecPivot = pObject ? pObject->ppFrameList[iLimb]->vecPivot : GGVECTOR3(0, 0, 0);
	 lua_pushnumber(L, vecPivot.x);
	 lua_pushnumber(L, vecPivot.y);
	 lua_pushnumber(L, vecPivot.z);
	 return 3;
 }
 // grows a box, in the space of the limb asked about, by a limb's mesh box and with bChildren by those of the limbs under it
 // (their rest pose: each child's matOriginal carries it into its parent's space)
 static void LuaAddLimbBounds ( sFrame* pFrame, GGMATRIX& matToLimb, bool bChildren, GGVECTOR3& vecMin, GGVECTOR3& vecMax, bool& bAny )
 {
	 sMesh* pMesh = pFrame->pMesh;
	 if ( pMesh && pMesh->dwVertexCount > 0 )
	 {
		 for ( int iCorner = 0; iCorner < 8; iCorner++ )
		 {
			 GGVECTOR3 vecCorner = GGVECTOR3 ( (iCorner & 1) ? pMesh->Collision.vecMax.x : pMesh->Collision.vecMin.x,
											   (iCorner & 2) ? pMesh->Collision.vecMax.y : pMesh->Collision.vecMin.y,
											   (iCorner & 4) ? pMesh->Collision.vecMax.z : pMesh->Collision.vecMin.z );
			 GGVec3TransformCoord ( &vecCorner, &vecCorner, &matToLimb );
			 if ( !bAny ) { vecMin = vecCorner; vecMax = vecCorner; bAny = true; }
			 vecMin.x = min ( vecMin.x, vecCorner.x ); vecMin.y = min ( vecMin.y, vecCorner.y ); vecMin.z = min ( vecMin.z, vecCorner.z );
			 vecMax.x = max ( vecMax.x, vecCorner.x ); vecMax.y = max ( vecMax.y, vecCorner.y ); vecMax.z = max ( vecMax.z, vecCorner.z );
		 }
	 }
	 if ( bChildren )
	 {
		 for ( sFrame* pChild = pFrame->pChild; pChild; pChild = pChild->pSibling )
		 {
			 GGMATRIX matChildToLimb = pChild->matOriginal * matToLimb;
			 LuaAddLimbBounds ( pChild, matChildToLimb, true, vecMin, vecMax, bAny );
		 }
	 }
 }
 // GetEntityLimbBounds(e, limb [, children]): minx, miny, minz, maxx, maxy, maxz of the limb's mesh in its own space; with
 // children 1 also the limbs under it (a rotor frame with blade limbs), nothing if there is no mesh
 int GetEntityLimbBounds ( lua_State* L )
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if ( n < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 bool bChildren = ( n >= 3 && lua_tointeger(L, 3) != 0 );
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( !pObject ) return 0;
	 GGMATRIX matIdentity;
	 GGMatrixIdentity ( &matIdentity );
	 GGVECTOR3 vecMin = GGVECTOR3(0, 0, 0), vecMax = GGVECTOR3(0, 0, 0);
	 bool bAny = false;
	 LuaAddLimbBounds ( pObject->ppFrameList[iLimb], matIdentity, bChildren, vecMin, vecMax, bAny );
	 if ( !bAny ) return 0;
	 lua_pushnumber(L, vecMin.x);
	 lua_pushnumber(L, vecMin.y);
	 lua_pushnumber(L, vecMin.z);
	 lua_pushnumber(L, vecMax.x);
	 lua_pushnumber(L, vecMax.y);
	 lua_pushnumber(L, vecMax.z);
	 return 6;
 }
 // GetEntityLimbParent(e, limb): the limb this one hangs from, -1 for the top of the hierarchy or a bad id
 int GetEntityLimbParent ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 int iParent = -1;
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( pObject )
	 {
		 sFrame* pParent = pObject->ppFrameList[iLimb]->pParent;
		 for ( int i = 0; pParent && i < pObject->iFrameCount; i++ )
		 {
			 if ( pObject->ppFrameList[i] == pParent ) { iParent = i; break; }
		 }
	 }
	 lua_pushinteger(L, iParent);
	 return 1;
 }
 // GetEntityLimbBoneCount(e, limb): how many bones the limb's mesh is skinned to, 0 if it isn't skinned (or has no mesh).
 // A skinned mesh doesn't move with the limb calls; its bones do
 int GetEntityLimbBoneCount ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 int iCount = 0;
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( pObject && pObject->ppFrameList[iLimb]->pMesh ) iCount = pObject->ppFrameList[iLimb]->pMesh->dwBoneCount;
	 lua_pushinteger(L, iCount);
	 return 1;
 }
 // GetEntityLimbBone(e, limb, n): the limb that is bone n (0 to count - 1) of the limb's skinned mesh, found by the bone's
 // name as the renderer does, and its name; -1 and "" if there is none
 int GetEntityLimbBone ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 3 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 int iBone = lua_tointeger(L, 3);
	 int iBoneLimb = -1;
	 const char* pBoneName = "";
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( pObject )
	 {
		 sMesh* pMesh = pObject->ppFrameList[iLimb]->pMesh;
		 if ( pMesh && pMesh->pBones && iBone >= 0 && iBone < (int)pMesh->dwBoneCount )
		 {
			 for ( int i = 0; i < pObject->iFrameCount; i++ )
			 {
				 if ( pObject->ppFrameList[i] && stricmp ( pObject->ppFrameList[i]->szName, pMesh->pBones[iBone].szName ) == NULL )
				 {
					 iBoneLimb = i;
					 pBoneName = pObject->ppFrameList[i]->szName;
					 break;
				 }
			 }
		 }
	 }
	 lua_pushinteger(L, iBoneLimb);
	 lua_pushstring(L, pBoneName);
	 return 2;
 }
 // SetEntityLimbAlpha(e, limb, percent [, children]): fades the limb, 0 to 100 (100 = as made), with children 1 also the limbs
 // under it (a rotor frame with blade limbs). The renderer dithers the fade rather than blending it, and a faded limb keeps
 // its shadow, so hide a limb that has faded out (HideLimb). A skinned mesh fades as its own limb, not through its bones
 int SetEntityLimbAlpha ( lua_State* L )
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if ( n < 3 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 bool bChildren = ( n >= 4 && lua_tointeger(L, 4) != 0 );
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( !pObject ) return 0;
	 WickedCall_SetLimbAlpha ( pObject, iLimb, lua_tonumber(L, 3) / 100.0f, bChildren );
	 return 0;
 }
 // GetEntityLimbAlpha(e, limb): the limb's alpha, 0 to 100, or that of the first limb under it with a mesh; 100 if none
 int GetEntityLimbAlpha ( lua_State* L )
 {
	 lua = L;
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 float fAlpha = 1.0f;
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( pObject ) fAlpha = WickedCall_GetLimbAlpha ( pObject, iLimb );
	 lua_pushnumber(L, fAlpha * 100.0f);
	 return 1;
 }

 // Entity Animation
 int SetEntityAnimation(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iAnimationSetIndex = lua_tonumber(L, 2);
	int iAnimationSetStart = lua_tonumber(L, 3);
	int iAnimationSetFinish = lua_tonumber(L, 4);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iEntID = t.entityelement[iEntityIndex].bankindex;
		if ( iEntID > 0 )
		{
			t.entityanim[iEntID][iAnimationSetIndex].start = iAnimationSetStart;
			t.entityanim[iEntID][iAnimationSetIndex].finish = iAnimationSetFinish;
			if ( iAnimationSetStart == -1 && iAnimationSetFinish == -1 )
				t.entityanim[iEntID][iAnimationSetIndex].found = 0;
			else
				t.entityanim[iEntID][iAnimationSetIndex].found = 1;
		}
	}
	return 1;
 }
 int GetEntityAnimationStart(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iAnimationSetIndex = lua_tonumber(L, 2);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iEntID = t.entityelement[iEntityIndex].bankindex;
		if ( iEntID > 0 )
		{
			iReturnValue = t.entityanim[iEntID][iAnimationSetIndex].start;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetEntityAnimationFinish(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iAnimationSetIndex = lua_tonumber(L, 2);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iEntID = t.entityelement[iEntityIndex].bankindex;
		if ( iEntID > 0 )
		{
			iReturnValue = t.entityanim[iEntID][iAnimationSetIndex].finish;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetEntityAnimationFound(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iAnimationSetIndex = lua_tonumber(L, 2);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iEntID = t.entityelement[iEntityIndex].bankindex;
		if ( iEntID > 0 )
		{
			iReturnValue = t.entityanim[iEntID][iAnimationSetIndex].found;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }

 int GetObjectAnimationFinished(lua_State *L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1 || n > 2) return 0;
	 int iReturnValue = 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 int iChopFramesOffThEnd = lua_tonumber(L, 2);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 int iObjID = t.entityelement[iEntityIndex].obj;
		 if (iObjID)
		 {
			 sObject* pObject = GetObjectData(iObjID);
			 if (pObject)
			 {
				// detects when a played animation comes to an end
				#ifdef WICKEDENGINE
				pObject->fAnimFrame = WickedCall_GetObjectFrame(pObject);
				#endif
				if (pObject->fAnimFrame >= t.smoothanim[iObjID].fn - iChopFramesOffThEnd)
				{
					iReturnValue = 1;
				}

				// detects when animation stopped
				if (WickedCall_GetObjectPlaying(pObject)==false)
				{
					iReturnValue = 1;
				}
			 }
		 }
	 }
	 lua_pushinteger (L, iReturnValue);
	 return 1;
 }

 int AdjustLookAimSettings (lua_State *L, int iMode )
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 t.e = lua_tonumber(L, 1);
	 float fValue = lua_tonumber(L, 2);
	 entity_lua_findcharanimstate();
	 if (t.tcharanimindex != -1)
	 {
		 switch (iMode)
		 {
			 case 1:  t.charanimstates[t.tcharanimindex].neckRightAndLeftLimit = fValue; break;
			 case 2:  t.charanimstates[t.tcharanimindex].neckRightAndLeftOffset = fValue; break;
			 case 3:  t.charanimstates[t.tcharanimindex].neckUpAndDownLimit = fValue; break;
			 case 4:  t.charanimstates[t.tcharanimindex].neckUpAndDownOffset = fValue; break;
			 case 5:  t.charanimstates[t.tcharanimindex].spineRightAndLeftLimit = fValue; break;
			 case 6:  t.charanimstates[t.tcharanimindex].spineRightAndLeftOffset = fValue; break;
			 case 7:  t.charanimstates[t.tcharanimindex].spineUpAndDownLimit = fValue; break;
			 case 8:  t.charanimstates[t.tcharanimindex].spineUpAndDownOffset = fValue; break;
		 }
	 }
	 return 1;
 }
 int AdjustLookSettingHorizLimit(lua_State *L) { return AdjustLookAimSettings (L, 1); }
 int AdjustLookSettingHorizOffset(lua_State *L) { return AdjustLookAimSettings (L, 2); }
 int AdjustLookSettingVertLimit(lua_State *L) { return AdjustLookAimSettings (L, 3); }
 int AdjustLookSettingVertOffset(lua_State *L) { return AdjustLookAimSettings (L, 4); }
 int AdjustAimSettingHorizLimit(lua_State *L) { return AdjustLookAimSettings (L, 5); }
 int AdjustAimSettingHorizOffset(lua_State *L) { return AdjustLookAimSettings (L, 6); }
 int AdjustAimSettingVertLimit(lua_State *L) { return AdjustLookAimSettings (L, 7); }
 int AdjustAimSettingVertOffset(lua_State *L) { return AdjustLookAimSettings (L, 8); }

 int GetEntityFootfallMax(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iEntID = t.entityelement[iEntityIndex].bankindex;
		if ( iEntID > 0 )
		{
			iReturnValue = t.entityprofile[iEntID].footfallmax;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetEntityFootfallKeyframe(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iFootfallIndex = lua_tonumber(L, 2);
	int iLeftOrRight = lua_tonumber(L, 3);
	if ( LuaEntityIDValid ( L, iEntityIndex, 1 ) )
	{
		int iEntID = t.entityelement[iEntityIndex].bankindex;
		if ( iEntID > 0 )
		{
			if ( iLeftOrRight == 0 )
				iReturnValue = t.entityfootfall[iEntID][iFootfallIndex].leftfootkeyframe;
			else
				iReturnValue = t.entityfootfall[iEntID][iFootfallIndex].rightfootkeyframe;
		}
	}
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }

 int GetEntityAnimationNameExistCore(lua_State *L, int iAnimQueryMode)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iReturnValue = 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 const char* pAnimationName = lua_tostring(L, 2);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 float fFoundStart = -1, fFoundFinish = -1;
		 cstr pStr = (LPSTR)pAnimationName;
		 if (iAnimQueryMode == 2)
		 {
			 // find any anim that anim exists and matches keyword, and is playing
			 char pSearchKeyword[MAX_PATH];
			 strcpy (pSearchKeyword, pStr.Get());
			 strlwr(pSearchKeyword);
			 int iObj = t.entityelement[iEntityIndex].obj;
			 if (iObj > 0)
			 {
				 sObject* pObject = GetObjectData(iObj);
				 sAnimationSet* pAnimSet = pObject->pAnimationSet;
				 while (pAnimSet)
				 {
					 char pAnimSetNameLower[MAX_PATH];
					 strcpy (pAnimSetNameLower, pAnimSet->szName);
					 strlwr(pAnimSetNameLower);
					 if (strstr(pAnimSetNameLower, pSearchKeyword) != NULL)
					 {
						 // found this keyword inside this anim name
						 extern int entity_lua_getanimationname(int, cstr, float*, float*);
						 int iAnimSetIndex = entity_lua_getanimationname (iEntityIndex, pAnimSet->szName, &fFoundStart, &fFoundFinish);
						 if (iAnimSetIndex > 0)
						 {
							 float fCurrentFrame = WickedCall_GetObjectFrame(pObject);
							 if (fCurrentFrame >= fFoundStart && fCurrentFrame <= fFoundFinish)
							 {
								 iReturnValue = iAnimSetIndex;
								 break;
							 }
						 }
					 }
					 if (iReturnValue > 0) break;
					 pAnimSet = pAnimSet->pNext;
				 }
			 }
		 }
		 else
		 {
			 extern int entity_lua_getanimationname(int, cstr, float*, float*);
			 int iAnimSetIndex = entity_lua_getanimationname (iEntityIndex, pStr, &fFoundStart, &fFoundFinish);
			 if (iAnimSetIndex > 0)
			 {
				 if (iAnimQueryMode == 1)
				 {
					 // anim exists, and is playing?
					 int iObj = t.entityelement[iEntityIndex].obj;
					 if (iObj > 0)
					 {
						 sObject* pObject = GetObjectData(iObj);
						 float fCurrentFrame = WickedCall_GetObjectFrame(pObject);
						 if (fCurrentFrame >= fFoundStart && fCurrentFrame <= fFoundFinish)
						 {
							 iReturnValue = iAnimSetIndex;
						 }
					 }
				 }
				 else
				 {
					 // anim exists
					 iReturnValue = iAnimSetIndex;
				 }
			 }
		 }
	 }
	 lua_pushinteger (L, iReturnValue);
	 return 1;
 }

 int GetEntityAnimationNameExist(lua_State *L)
 {
	 int iAnimQueryMode = 0;
	 return GetEntityAnimationNameExistCore(L, iAnimQueryMode);
 }

 int GetEntityAnimationNameExistAndPlaying(lua_State *L)
 {
	 int iAnimQueryMode = 1;
	 return GetEntityAnimationNameExistCore(L, iAnimQueryMode);
 }
 int GetEntityAnimationNameExistAndPlayingSearchAny(lua_State* L)
 {
	 int iAnimQueryMode = 2;
	 return GetEntityAnimationNameExistCore(L, iAnimQueryMode);
 }

 int GetEntityAnimationTriggerFrame(lua_State *L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iReturnValue = -1;
	 int iEntityIndex = lua_tonumber(L, 1);
	 int iTriggerIndex = lua_tonumber(L, 2);
	 if (LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		int iObj = t.entityelement[iEntityIndex].obj;
		if (iObj > 0)
		{
			sObject* pObject = GetObjectData(iObj);
			if (pObject)
			{
				float fCurrentStart = t.smoothanim[iObj].st;
				float fCurrentFinish = t.smoothanim[iObj].fn;
				sAnimationSet* pAnimSet = pObject->pAnimationSet;
				while (pAnimSet)
				{
					// find correct animation set using current start/finish
					if (fCurrentStart == pAnimSet->fAnimSetStart && fCurrentFinish == pAnimSet->fAnimSetFinish)
					{
						 int iKeyFrame = 0;
						 if (iTriggerIndex == 1) iKeyFrame = (int)pAnimSet->fAnimSetStep1;
						 if (iTriggerIndex == 2) iKeyFrame = (int)pAnimSet->fAnimSetStep2;
						 if (iTriggerIndex == 3) iKeyFrame = (int)pAnimSet->fAnimSetStep3;
						 iReturnValue = iKeyFrame;
						 break;
					 }
					pAnimSet = pAnimSet->pNext;
				}
			 }
		 }
	 }
	 lua_pushinteger (L, iReturnValue);
	 return 1;
 }

 int GetEntityAnimationStartFinish(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iReturnValue = 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 const char* pAnimationName = lua_tostring(L, 2);
	 float fFoundStart = -1, fFoundFinish = -1;
	 if (iEntityIndex > 0)
	 {
		 cstr pStr = (LPSTR)pAnimationName;
		 extern int entity_lua_getanimationname(int, cstr, float*, float*);
		 int iAnimSetIndex = entity_lua_getanimationname (iEntityIndex, pStr, &fFoundStart, &fFoundFinish);
	 }
	 lua_pushinteger (L, (int)fFoundStart);
	 lua_pushinteger (L, (int)fFoundFinish);
	 return 2;
 }

 // Entity creation and destruction

 int GetOriginalEntityElementMax(lua_State* L)
 {
	 lua = L;
	 int iEntityCount = g.entityelementlist;
	 lua_pushinteger (L, iEntityCount);
	 return 1;
 }

 int CreateEntityIfNotPresent(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iNewE = -1;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (iEntityIndex > 0)
	 {
		 if (iEntityIndex >= t.entityelement.size())
		 {
			 Dim (t.storeentityelement, g.entityelementmax);
			 for (t.e = 1; t.e <= g.entityelementmax; t.e++)
			 {
				 t.storeentityelement[t.e] = t.entityelement[t.e];
			 }
			 UnDim (t.entityelement);
			 UnDim (t.entityshadervar);
			 int iOldMaxCount = g.entityelementmax;
			 g.entityelementmax = iEntityIndex + 10;
			 Dim (t.entityelement, g.entityelementmax);
			 Dim2(t.entityshadervar, g.entityelementmax, g.globalselectedshadermax);
			 for (t.e = 1; t.e <= iOldMaxCount; t.e++)
			 {
				 t.entityelement[t.e] = t.storeentityelement[t.e];
			 }
		 }
		 if (iEntityIndex > g.entityelementlist) g.entityelementlist = iEntityIndex;
	 }
 }

 int SpawnNewEntityCore(int iEntityIndex)
 {
	#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
	#endif
	int iNewE = -1;
	t.bSpawnCalledFromLua = true;
	int storee = t.e;
	int storeentid = t.entid;
	int entid = t.entityelement[iEntityIndex].bankindex;
	t.gridentity = entid;
	t.gridentityeditorfixed = 0;
	t.gridentitystaticmode = t.entityelement[iEntityIndex].staticflag;
	t.gridentityposx_f = t.entityelement[iEntityIndex].x;
	t.gridentityposy_f = t.entityelement[iEntityIndex].y;
	t.gridentityposz_f = t.entityelement[iEntityIndex].z;
	t.gridentityrotatex_f = t.entityelement[iEntityIndex].rx;
	t.gridentityrotatey_f = t.entityelement[iEntityIndex].ry;
	t.gridentityrotatez_f = t.entityelement[iEntityIndex].rz;
	t.gridentityrotatequatmode = t.entityelement[iEntityIndex].quatmode;
	t.gridentityrotatequatx_f = t.entityelement[iEntityIndex].quatx;
	t.gridentityrotatequaty_f = t.entityelement[iEntityIndex].quaty;
	t.gridentityrotatequatz_f = t.entityelement[iEntityIndex].quatz;
	t.gridentityrotatequatw_f = t.entityelement[iEntityIndex].quatw;
	t.gridentityscalex_f = ObjectScaleX(t.entityelement[iEntityIndex].obj);
	t.gridentityscaley_f = ObjectScaleY(t.entityelement[iEntityIndex].obj);
	t.gridentityscalez_f = ObjectScaleZ(t.entityelement[iEntityIndex].obj);
	t.entid = entid; entity_fillgrideleproffromprofile();
	//LB: an copy over material changes from the cloned entiy element
	t.grideleprof.WEMaterial = t.entityelement[iEntityIndex].eleprof.WEMaterial;
	// and a weapon's settings as placed: entity_addentitytomap writes these into the gun's first fire mode and every
	// placed copy of the gun, so the profile's defaults would undo the level's edits
	t.grideleprof.damage = t.entityelement[iEntityIndex].eleprof.damage;
	t.grideleprof.accuracy = t.entityelement[iEntityIndex].eleprof.accuracy;
	t.grideleprof.reloadqty = t.entityelement[iEntityIndex].eleprof.reloadqty;
	t.grideleprof.fireiterations = t.entityelement[iEntityIndex].eleprof.fireiterations;
	t.grideleprof.range = t.entityelement[iEntityIndex].eleprof.range;
	t.grideleprof.dropoff = t.entityelement[iEntityIndex].eleprof.dropoff;
	t.grideleprof.usespotlighting = t.entityelement[iEntityIndex].eleprof.usespotlighting;
	t.grideleprof.clipcapacity = t.entityelement[iEntityIndex].eleprof.clipcapacity;
	t.grideleprof.weaponpropres1 = t.entityelement[iEntityIndex].eleprof.weaponpropres1;
	t.grideleprof.weaponpropres2 = t.entityelement[iEntityIndex].eleprof.weaponpropres2;
	t.grideleprof.lifespan = t.entityelement[iEntityIndex].eleprof.lifespan;
	t.grideleprof.throwspeed = t.entityelement[iEntityIndex].eleprof.throwspeed;
	t.grideleprof.throwangle = t.entityelement[iEntityIndex].eleprof.throwangle;
	t.grideleprof.bounceqty = t.entityelement[iEntityIndex].eleprof.bounceqty;
	t.grideleprof.explodeonhit = t.entityelement[iEntityIndex].eleprof.explodeonhit;
	
	extern bool bNextObjectMustBeClone;
	bNextObjectMustBeClone = true;
	
	entity_addentitytomap ();
	
	bNextObjectMustBeClone = false;

	t.e = t.tupdatee;
	t.entityelement[t.e].eleprof = t.entityelement[iEntityIndex].eleprof;
	t.entityelement[t.e].scalex = t.entityelement[iEntityIndex].scalex;
	t.entityelement[t.e].scaley = t.entityelement[iEntityIndex].scaley;
	t.entityelement[t.e].scalez = t.entityelement[iEntityIndex].scalez;
	t.entityelement[t.e].soundset = t.entityelement[iEntityIndex].soundset;
	t.entityelement[t.e].soundset1 = t.entityelement[iEntityIndex].soundset1;
	t.entityelement[t.e].soundset2 = t.entityelement[iEntityIndex].soundset2;
	t.entityelement[t.e].soundset3 = t.entityelement[iEntityIndex].soundset3;
	t.entityelement[t.e].soundset4 = t.entityelement[iEntityIndex].soundset4;
	t.entityelement[t.e].soundset5 = t.entityelement[iEntityIndex].soundset5;
	t.entityelement[t.e].soundset6 = t.entityelement[iEntityIndex].soundset6;
	t.entityelement[t.e].iAllowBuletHole = t.entityelement[iEntityIndex].iAllowBuletHole;
	// clones always show at start
	t.entityelement[t.e].eleprof.spawnatstart = 1;
	iNewE = t.e;
	extern bool g_bSpawningThisOneNow;
	g_bSpawningThisOneNow = true;
	physics_prepareentityforphysics ();
	g_bSpawningThisOneNow = false;
	t.entityelement[t.e].lua.firsttime = 0;
	// clones need parent health at least top begin with
	t.entityelement[t.e].health = t.entityelement[iEntityIndex].health;
	// special limbo mode to skip activating this entity until next lua_begin cycle
	t.entityelement[iNewE].active = 0;
	t.entityelement[iNewE].lua.flagschanged = 123;
	t.entityelement[iNewE].iWasSpawnedInGame = iEntityIndex;
	t.e = storee;
	t.entid = storeentid;
	t.gridentity = 0;
	t.bSpawnCalledFromLua = false;
	return iNewE;
 }
 
 std::vector<int> vSpawnList;

 int SpawnNewEntity(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iNewE = -1;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (iEntityIndex > 0 && LuaEntityIDValid(L, iEntityIndex, 1))
	 {
		 char pMsg[256];
		 if (t.entityelement[iEntityIndex].bankindex > 0)
		 {
			 int iArraySizeBefore = g.entityelementmax;
			 iNewE = SpawnNewEntityCore(iEntityIndex);
			 vSpawnList.push_back(iNewE);
			 sprintf(pMsg, "SpawnNewEntityCore : %d from %d (entity array %d%s)", iNewE, iEntityIndex, g.entityelementmax, g.entityelementmax != iArraySizeBefore ? ", grown" : "");
		 }
		 else
		 {
			 // a deleted spawn's slot has no entity bank entry to copy
			 sprintf(pMsg, "SpawnNewEntity ignored %d: no entity in that slot", iEntityIndex);
		 }
		 timestampactivity(0, pMsg);
	 }
	 lua_pushinteger (L, iNewE);
	 return 1;
 }

 void CleanUpSpawedObject(void)
 {
	 int storee = t.e;
	 int storeentid = t.entid;
	 for (int i = 0; i < vSpawnList.size(); i++)
	 {
		 int iEntityIndex = vSpawnList[i];
		 if (iEntityIndex > t.storedentityelementlist && iEntityIndex < t.entityelement.size())
		 {
			 // was created at run-time, can delete
			 t.tentitytoselect = iEntityIndex;
			 t.entid = t.entityelement[t.tentitytoselect].bankindex;
			 if (t.entityelement[t.tentitytoselect].obj > 0)
			 {
				 if (t.entityelement[t.tentitytoselect].usingphysicsnow == 1)
				 {
					 t.tphyobj = t.entityelement[t.tentitytoselect].obj;
					 physics_disableobject();
					 t.entityelement[t.tentitytoselect].usingphysicsnow = 0;
				 }
				 t.entityelement[t.tentitytoselect].editorlock = 0;
				 for (g.charanimindex = 1; g.charanimindex <= g.charanimindexmax; g.charanimindex++)
				 {
					 if (t.tentitytoselect == t.charanimstates[g.charanimindex].e)
					 {
						 t.charanimstates[g.charanimindex].e = 0;
						 t.charanimstates[g.charanimindex].obj = 0;
					 }
				 }
				 if (t.entityelement[t.tentitytoselect].attachmentobj > 0)
				 {
					 HideObject(t.entityelement[t.tentitytoselect].attachmentobj);
					 t.entityelement[t.tentitytoselect].attachmentobj = 0;
				 }
				 entity_deleteentityfrommap();
				 if (t.entityelement[t.tentitytoselect].ragdollified == 1)
				 {
					 //t.tphyobj = t.entityelement[t.tentitytoselect].obj; ragdoll_destroy ();
					 t.entityelement[t.tentitytoselect].ragdollified = 0;
				 }
			 }
		 }
	 }
	 vSpawnList.clear();
	 t.e = storee;
	 t.entid = storeentid;
 }


 int DeleteNewEntity(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (iEntityIndex > 0)
	 {
		 if (iEntityIndex > t.storedentityelementlist)
		 {
			 // was created at run-time, can delete
			 int storee = t.e;
			 int storeentid = t.entid;
			 t.tentitytoselect = iEntityIndex;
			 t.entid = t.entityelement[t.tentitytoselect].bankindex;
			 if (t.entityelement[t.tentitytoselect].usingphysicsnow == 1)
			 {
				 t.tphyobj = t.entityelement[t.tentitytoselect].obj;
				 physics_disableobject ();
				 t.entityelement[t.tentitytoselect].usingphysicsnow = 0;
			 }
			 t.entityelement[t.tentitytoselect].editorlock = 0;
			 for (g.charanimindex = 1; g.charanimindex <= g.charanimindexmax; g.charanimindex++)
			 {
				 if (t.tentitytoselect == t.charanimstates[g.charanimindex].e)
				 {
					 t.charanimstates[g.charanimindex].e = 0; 
					 t.charanimstates[g.charanimindex].obj = 0;
				 }
			 }
			 if (t.entityelement[t.tentitytoselect].attachmentobj > 0)
			 {
				 HideObject (t.entityelement[t.tentitytoselect].attachmentobj);
				 t.entityelement[t.tentitytoselect].attachmentobj = 0;
			 }
			 entity_deleteentityfrommap ();
			 if (t.entityelement[t.tentitytoselect].ragdollified == 1)
			 {
				 //t.tphyobj = t.entityelement[t.tentitytoselect].obj; ragdoll_destroy ();
				 t.entityelement[t.tentitytoselect].ragdollified = 0;
			 }


			 auto it = std::find(vSpawnList.begin(), vSpawnList.end(), iEntityIndex);
			 if (it != vSpawnList.end()) {
				 vSpawnList.erase(it);
			 }

			 t.e = storee;
			 t.entid = storeentid;
		 }
	 }
	 return 0;
 }

 // Other stuff

 int GetAmmoClipMax(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	int iReturnValue = -1;
	if ( t.tcharanimindex != - 1 ) iReturnValue = t.charanimstates[t.tcharanimindex].ammoinclipmax;
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int GetAmmoClip(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	int iReturnValue = -1;
	if ( t.tcharanimindex != - 1 ) iReturnValue = t.charanimstates[t.tcharanimindex].ammoinclip;
	lua_pushinteger ( L, iReturnValue );
	return 1;
 }
 int SetAmmoClip(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	if ( t.tcharanimindex != - 1 ) t.charanimstates[t.tcharanimindex].ammoinclip = lua_tonumber(L, 2);
	return 0;
 }

 //
 // Entity Physics Commands
 //
 int FreezeEntityCore ( lua_State *L, int iCoreMode )
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if ( !LuaEntityIDValid ( L, iEntityIndex, 1 ) ) return 0;
	int iObjectNumber = t.entityelement[iEntityIndex].obj;
	if ( iObjectNumber > 0 )
	{
		sObject* pObject = GetObjectData ( iObjectNumber );
		if ( pObject )
		{
			if ( iCoreMode == 1 )
			{
				// force a freeze mode onto physics object (FreezeEntity(e) alone did nothing: it now means mode 0)
				int iFreezeMode = 0;
				if ( n >= 2 ) iFreezeMode = lua_tonumber(L, 2);
				ODESetBodyResponse ( iObjectNumber, 1 + iFreezeMode );
			}
			else
			{
				// restore response to time of creation
				ODESetBodyResponse ( iObjectNumber, 0 );
			}
		}
	}
	return 0;
 }
 int FreezeEntity(lua_State *L) { return FreezeEntityCore ( L, 1 ); }
 int UnFreezeEntity(lua_State *L) { return FreezeEntityCore ( L, 0 ); }

 // Terrain
 float GetLUATerrainHeightEx ( float fX, float fZ )
 {
	float fReturnHeight = g.gdefaultterrainheight;
	fReturnHeight = BT_GetGroundHeight (0, fX, fZ);
	return fReturnHeight;
 }
 int GetTerrainHeight(lua_State *L)
 {
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	float fReturnHeight = 0.0f;
	float fX = lua_tonumber(L, 1);
	float fZ = lua_tonumber(L, 2);
	fReturnHeight = GetLUATerrainHeightEx(fX,fZ);
	lua_pushinteger ( L, fReturnHeight );
	return 1;
 }
 int GetTerrainHeightFloat(lua_State* L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 float fReturnHeight = 0.0f;
	 float fX = lua_tonumber(L, 1);
	 float fZ = lua_tonumber(L, 2);
	 fReturnHeight = GetLUATerrainHeightEx(fX, fZ);
	 lua_pushnumber(L, fReturnHeight);
	 return 1;
 }

 /* Converted code. The below LUA used int and produce a bad result tangle always 0 , 22 , 44 ... try Prompt(tangle)
			-- Determine slope angle of plr direction
			ttfinalplrmovey=GetGamePlayerControlMovey()
 			ttplrx=GetPlrObjectPositionX()
			ttplry=GetPlrObjectPositionY()
			ttplrz=GetPlrObjectPositionZ()
			ttplrgroundy=GetSurfaceHeight(ttplrx,ttplry,ttplrz)		
			ttplrx2=NewXValue(ttplrx,ttfinalplrmovey,1.0)
			ttplrz2=NewZValue(ttplrz,ttfinalplrmovey,1.0)
			ttplrgroundy2=GetSurfaceHeight(ttplrx2,ttplry,ttplrz2)		
			-- and angle movement vector for better player control
			local tangle = ((ttplrgroundy2-ttplrgroundy)/1.0)*22.0
			if tangle < -45 then tangle = -45 end
			if tangle > 45 then tangle = 45 end
			SetGamePlayerStateJetpackVerticalMove(tangle)
			Prompt(tangle)
 
 */

 int SetPlayerSlopeAngle(lua_State *L)
 {
	 //PE: Now use float for more precise angle and only update at 20 fps.
	 static float lasttangle = 0.0;
	 static int iUpdateTimer = 0.0;
	 #ifdef _DEBUG
	 //in debug, this is slow enough to ALWAYS call rays (450ms per call in debug)!!
	 //if (Timer() > iUpdateTimer + 55) //PE: 1000/20 = 50 (20 fps).
	 return 0;
	 #else
	 if (Timer() > iUpdateTimer + 55) //PE: 1000/20 = 50 (20 fps).
	 {
		 iUpdateTimer = Timer();
	 }
	 else
	 {
		 //PE: Use last value.
		 t.tjetpackverticalmove_f = lasttangle;
		 return(0);
	 }
	 #endif

	 // instead, use nav mesh which accounts for terrain AND objects on it, no need for massive obj scan
	 float HitY1 = 0.0;
	 float HitY2 = 0.0;
	 float fX = ObjectPositionX(t.aisystem.objectstartindex);
	 float fY = ObjectPositionY(t.aisystem.objectstartindex);
	 float fZ = ObjectPositionZ(t.aisystem.objectstartindex);
	 float fHitX, fHitY, fHitZ;
	 fHitY = 0.0f;
	 float fMargin = 5.0f;
	 float fMaximumDrop = 100.0f;
	 bool bFailed = false;
	 if (g_RecastDetour.isWithinNavMesh(fX, fY + fMargin, fZ) == true)
	 {
		 HitY1 = g_RecastDetour.getYFromPos(fX, fY + fMargin, fZ);
	 }
	 else
	 {
		 bFailed = true;
	 }
	 if (!bFailed)
	 {
		 float fX2 = NewXValue(fX, t.playercontrol.movey_f, 1.0);
		 float fZ2 = NewZValue(fZ, t.playercontrol.movey_f, 1.0);
		 if (g_RecastDetour.isWithinNavMesh(fX2, fY + fMargin, fZ2) == true)
		 {
			 HitY2 = g_RecastDetour.getYFromPos(fX2, fY + fMargin, fZ2);
		 }
		 else
		 {
			 bFailed = true;
		 }
		 float tangle = 0.0;
		 if (!bFailed)
		 {
			 tangle = ((HitY2 - HitY1) / 1.0) * 22.0;
			 if (tangle < -45) tangle = -45;
			 if (tangle > 45) tangle = 45;
			 lasttangle = tangle;
		 }
		 if (bFailed) tangle = lasttangle;
		 t.tjetpackverticalmove_f = tangle;
	 }

	 /* horrendously slow even in non-opt release mode
	 float HitY1 = 0.0;
	 float HitY2 = 0.0;
	 float fX = ObjectPositionX(t.aisystem.objectstartindex);
	 float fY = ObjectPositionY(t.aisystem.objectstartindex);
	 float fZ = ObjectPositionZ(t.aisystem.objectstartindex);
	 float fHitX, fHitY, fHitZ;
	 fHitY = 0.0f;
	 float fMargin = 5.0f;
	 float fMaximumDrop = 100.0f;
	 bool bFailed = false;
	 if (WickedCall_SentRay2(fX, fY + fMargin, fZ, 0, -1.0f, 0, &fHitX, &fHitY, &fHitZ, NULL, NULL, NULL, NULL, GGRENDERLAYERS_TERRAIN | GGRENDERLAYERS_NORMAL) == true)
	 {
		 HitY1 = fHitY;
	 }
	 else
	 {
		 //PE: Failed.
		 bFailed = true;
	 }
	 if (!bFailed)
	 {
		 float fX2 = NewXValue(fX, t.playercontrol.movey_f, 1.0);
		 float fZ2 = NewZValue(fZ, t.playercontrol.movey_f, 1.0);
		 if (WickedCall_SentRay2(fX2, fY + fMargin, fZ2, 0, -1.0f, 0, &fHitX, &fHitY, &fHitZ, NULL, NULL, NULL, NULL, GGRENDERLAYERS_TERRAIN | GGRENDERLAYERS_NORMAL) == true)
		 {
			 HitY2 = fHitY;
		 }
		 else
		 {
			 bFailed = true;
		 }

		 float tangle = 0.0;
		 if (!bFailed)
		 {
			 tangle = ((HitY2 - HitY1) / 1.0) *22.0;
			 if (tangle < -45) tangle = -45;
			 if (tangle > 45) tangle = 45;
			 lasttangle = tangle;
		 }

		 if (bFailed) tangle = lasttangle;

		 t.tjetpackverticalmove_f = tangle;
	 }
	 */
	 return 0;
 }

 int GetSurfaceHeight(lua_State *L)
 {
	 lua = L;
	 int n = LUA_GETTOP(L);
	 if (n < 3) return 0;
	 float fReturnHeight = 0.0f;
	 int iHit = 0;
	 float fX = lua_tonumber(L, 1);
	 float fY = lua_tonumber(L, 2);
	 float fZ = lua_tonumber(L, 3);
	 #ifdef WICKEDENGINE
	 float fHitX, fHitY, fHitZ;
	 fHitY = 0.0f;
	 float fMargin = 5.0f;
	 float fMaximumDrop = 100.0f;
	 if (WickedCall_SentRay2(fX, fY + fMargin, fZ, 0, -1.0f, 0, &fHitX, &fHitY, &fHitZ, NULL, NULL, NULL, NULL, GGRENDERLAYERS_TERRAIN | GGRENDERLAYERS_NORMAL) == true)
	 {
		 fReturnHeight = fHitY;
		 iHit = 1;
	 }
	 #else
	 fReturnHeight = GetLUATerrainHeightEx(fX, fZ);
	 iHit = 1;
	 #endif
	 // the height as a number (lua_pushinteger cut it to a whole unit), and 1 if a surface was found below (0 on a miss,
	 // when the height is 0)
	 lua_pushnumber (L, fReturnHeight);
	 lua_pushinteger (L, iHit);
	 return 2;
 }

 // DarkAI
 int AISetEntityControl(lua_State *L)
 {
	 lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 2 ) return 0;

	#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	#else
	AISetEntityControl ( lua_tonumber(L, 1), lua_tonumber(L, 2) );
	#endif

	// TO DO: Dave - need to check if coop mode is on, need a command to let lua know not to send these messages if they are not needed
	SteamSendLua ( 30 , lua_tointeger(L, 1), lua_tonumber(L, 2) );

	return 0;
 }

 int AIEntityAssignPatrolPath(lua_State *L)
 {
	 lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 2 ) return 0;

#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
#else
	AIEntityAssignPatrolPath ( lua_tonumber(L, 1), lua_tonumber(L, 2) );
#endif

	return 0;
 }

 int AIEntityStop(lua_State *L)
 {
	 lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 1 ) return 0;

#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
#else
	AIEntityStop ( lua_tonumber(L, 1) );
#endif

	return 0;
 }

int AIEntityAddTarget(lua_State *L)
{
	 lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 2 ) return 0;

#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
#else
	AIEntityAddTarget ( lua_tonumber(L, 1), lua_tonumber(L, 2) );
#endif

	return 0;
}

int AIEntityRemoveTarget(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 2 ) return 0;

#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
#else
	AIEntityRemoveTarget ( lua_tonumber(L, 1), lua_tonumber(L, 2) );
#endif

	return 0;
}

int AIEntityMoveToCover(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 3 ) return 0;

#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushinteger ( L , AIEntityMoveToCover ( lua_tonumber(L, 1), lua_tonumber(L, 2) , lua_tonumber(L, 3) ) );
#endif

	return 1;
}

int AIGetEntityCanSee(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 5 ) return 0;

#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushinteger ( L , AIGetEntityCanSee ( lua_tonumber(L, 1), lua_tonumber(L, 2) , lua_tonumber(L, 3) , lua_tonumber(L, 4) , lua_tonumber(L, 5) ) );
#endif

	return 1;
}

int AIGetEntityCanFire(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 1 ) return 0;

#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushinteger ( L , AIGetEntityCanFire ( lua_tonumber(L, 1) ) );
#endif

	return 1;
}

int AIGetEntityViewRange(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AIGetEntityViewRange ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}
int AIGetEntitySpeed(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AIGetEntitySpeed ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}

int AIGetTotalPaths(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n != 0 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushinteger ( L , AIGetTotalPaths () );
#endif
	return 1;
}
int AIGetPathCountPoints(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushinteger ( L , AIGetPathCountPoints ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}
int AIPathGetPointX(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AIPathGetPointX ( lua_tonumber(L, 1) , lua_tonumber(L, 2) ) );
#endif
	return 1;
}
int AIPathGetPointY(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AIPathGetPointY ( lua_tonumber(L, 1) , lua_tonumber(L, 2) ) );
#endif
	return 1;
}
int AIPathGetPointZ(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AIPathGetPointZ ( lua_tonumber(L, 1) , lua_tonumber(L, 2) ) );
#endif
	return 1;
}

int AIGetTotalCover(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n != 0 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushinteger ( L , AIGetTotalCover () );
#endif
	return 1;
}
int AICoverGetPointX(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AICoverGetPointX ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}
int AICoverGetPointY(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AICoverGetPointY ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}
int AICoverGetPointZ(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AICoverGetPointZ ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}
int AICoverGetAngle(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	lua_pushnumber ( L , AICoverGetAngle ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}
int AICoverGetIfUsed(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushstring (L, "");
#else
	lua_pushstring ( L , AICoverGetIfUsed ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}

int MsgBox(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 1 ) return 0;

	MessageBox(NULL, lua_tostring(L, 1), "LUA MESSAGE", MB_TOPMOST | MB_OK);

	return 0;
}

int AISetEntityMoveBoostPriority(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	int iObj = lua_tointeger(L, 1);
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
#else
	AISetEntityMoveBoostPriority ( iObj );
#endif
	return 1;
}

int AIEntityGoToPosition(lua_State *L)
{
	// can pass in 3 or 4 params
	// (3) obj,x,z 
	// (4) obj,x,y,z
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	int iObj = lua_tointeger(L, 1);
	float fGoToX = lua_tonumber(L, 2);
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX: say so once in the log, as the call returns nothing and the entity does not move
	static bool bWarnedNoAI = false;
	if ( bWarnedNoAI == false )
	{
		bWarnedNoAI = true;
		char pMsg[256];
		strcpy ( pMsg, "AIEntityGoToPosition does nothing in MAX (no DarkAI): move with RDFindPath / StartMoveAndRotateToXYZ" );
		timestampactivity ( 0, pMsg );
	}
#else
	if ( n == 3 )
	{
		// legacy X and Z within current container
		float fGoToZ = lua_tonumber(L, 3);
		AIEntityGoToPosition ( iObj, fGoToX, fGoToZ );
	}
	if ( n == 4 )
	{
		// search for container based on true X, Y, Z position
		float fGoToY = lua_tonumber(L, 3);
		float fGoToZ = lua_tonumber(L, 4);
		int iDestinationContainerIndex = 0;
		t.tpointx_f = fGoToX;
		t.tpointz_f = fGoToZ;
		for ( t.waypointindex = 1; t.waypointindex <= g.waypointmax; t.waypointindex++ )
		{
			// zone - confined containers
			if ( t.waypoint[t.waypointindex].style == 3 )
			{
				t.tokay = 0; waypoint_ispointinzone ( );
				if ( t.tokay == 1 ) 
				{
					int e = t.waypoint[t.waypointindex].linkedtoentityindex;
					if ( fGoToY > t.entityelement[e].y - 25.0f && fGoToY < t.entityelement[e].y + 65.0f )
					{
						// only if Y position above zone entity position and below cap of this layer
						iDestinationContainerIndex = t.waypointindex;
					}
				}
			}
		}
		AIEntityGoToPosition ( iObj, fGoToX, fGoToZ, iDestinationContainerIndex );
	}
#endif
	return 0;
}


int AIGetEntityHeardSound(lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushnumber (L, 0);
#else
	lua_pushnumber ( L , AIGetEntityHeardSound ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}

int AISetData ( lua_State *L, int iDataMode )
{
	lua = L;
	int iParamNum = 0;
	switch ( iDataMode )
	{
		case 1 : iParamNum = 4;	break;
		case 2 : iParamNum = 2;	break;
	}
	int n = LUA_GETTOP(L);
	if ( n < iParamNum ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
#else
	switch ( iDataMode )
	{
		case 1 : AISetEntityPosition ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) ); break;
		case 2 : AISetEntityTurnSpeed ( lua_tonumber(L, 1), lua_tonumber(L, 2) ); break;
	}
#endif
	return 0;
}
int AIGetData ( lua_State *L, int iDataMode )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushnumber (L, 0);
#else
	switch ( iDataMode )
	{
		case 1 : lua_pushnumber ( L , AIGetEntityAngleY ( lua_tonumber(L, 1) ) ); break;
		case 2 : if ( t.aisystem.processlogic == 0 )
					lua_pushnumber ( L , 0 ); 
				 else
					lua_pushnumber ( L , AIGetEntityIsMoving ( lua_tonumber(L, 1) ) ); 
				 break;
	}
#endif
	return 1;
}
int AISetEntityPosition ( lua_State *L ) { return AISetData ( L, 1 ); }
int AISetEntityTurnSpeed ( lua_State *L ) { return AISetData ( L, 2 ); }
int AIGetEntityAngleY ( lua_State *L ) { return AIGetData ( L, 1 ); }
int AIGetEntityIsMoving ( lua_State *L ) { return AIGetData ( L, 2 ); }

//
// Visual Attribute Settings
//

int AIGetVisualSetting ( lua_State *L, int iMode )
{
	lua = L;
	//int n = LUA_GETTOP(L);
	//if ( n < 1 ) return 0;
	switch ( iMode )
	{
		case 1 : lua_pushnumber ( L, t.visuals.FogNearest_f ); break;
		case 2 : lua_pushnumber ( L, t.visuals.FogDistance_f ); break;
		case 3 : lua_pushnumber ( L, t.visuals.FogR_f ); break;
		case 4 : lua_pushnumber ( L, t.visuals.FogG_f ); break;
		case 5 : lua_pushnumber ( L, t.visuals.FogB_f ); break;
		case 6 : lua_pushnumber ( L, t.visuals.FogA_f ); break;
		case 7 : lua_pushnumber ( L, t.visuals.AmbienceIntensity_f ); break;
		case 8 : lua_pushnumber ( L, t.visuals.AmbienceRed_f ); break;
		case 9 : lua_pushnumber ( L, t.visuals.AmbienceGreen_f ); break;
		case 10 : lua_pushnumber ( L, t.visuals.AmbienceBlue_f ); break;
		case 11 : lua_pushnumber ( L, t.visuals.SurfaceRed_f ); break;
		case 12 : lua_pushnumber ( L, t.visuals.SurfaceGreen_f ); break;
		case 13 : lua_pushnumber ( L, t.visuals.SurfaceBlue_f ); break;
		case 14 : lua_pushnumber ( L, t.visuals.SurfaceIntensity_f ); break;
		case 15 : lua_pushnumber ( L, t.visuals.VignetteRadius_f ); break;
		case 16 : lua_pushnumber ( L, t.visuals.VignetteIntensity_f ); break;
		case 17 : lua_pushnumber ( L, t.visuals.MotionDistance_f ); break;
		case 18 : lua_pushnumber ( L, t.visuals.MotionIntensity_f ); break;
		case 19 : lua_pushnumber ( L, t.visuals.DepthOfFieldDistance_f ); break;
		case 20 : lua_pushnumber ( L, t.visuals.DepthOfFieldIntensity_f ); break;
		default : lua_pushnumber ( L, 0 ); break;
	}
	return 1;
}

int GetFogNearest(lua_State *L) { return AIGetVisualSetting ( L, 1 ); }
int GetFogDistance(lua_State *L) { return AIGetVisualSetting ( L, 2 ); }
int GetFogRed(lua_State *L) { return AIGetVisualSetting ( L, 3 ); }
int GetFogGreen(lua_State *L) { return AIGetVisualSetting ( L, 4 ); }
int GetFogBlue(lua_State *L) { return AIGetVisualSetting ( L, 5 ); }
int GetFogIntensity(lua_State *L) { return AIGetVisualSetting ( L, 6 ); }
int GetAmbienceIntensity(lua_State *L) { return AIGetVisualSetting ( L, 7 ); }
int GetAmbienceRed(lua_State *L) { return AIGetVisualSetting ( L, 8 ); }
int GetAmbienceGreen(lua_State *L) { return AIGetVisualSetting ( L, 9 ); }
int GetAmbienceBlue(lua_State *L) { return AIGetVisualSetting ( L, 10 ); }
int GetSurfaceRed(lua_State *L) { return AIGetVisualSetting ( L, 11 ); }
int GetSurfaceGreen(lua_State *L) { return AIGetVisualSetting ( L, 12 ); }
int GetSurfaceBlue(lua_State *L) { return AIGetVisualSetting ( L, 13 ); }
int GetSurfaceIntensity(lua_State *L) { return AIGetVisualSetting ( L, 14 ); }
int GetPostVignetteRadius(lua_State *L) { return AIGetVisualSetting ( L, 15 ); }
int GetPostVignetteIntensity(lua_State *L) { return AIGetVisualSetting ( L, 16 ); }
int GetPostMotionDistance(lua_State *L) { return AIGetVisualSetting ( L, 17 ); }
int GetPostMotionIntensity(lua_State *L) { return AIGetVisualSetting ( L, 18 ); }
int GetPostDepthOfFieldDistance(lua_State *L) { return AIGetVisualSetting ( L, 19 ); }
int GetPostDepthOfFieldIntensity(lua_State *L) { return AIGetVisualSetting ( L, 20 ); }

int AICouldSee(lua_State *L )
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 4 ) return 0;

#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
#else
	lua_pushinteger ( L , AICouldSee ( lua_tonumber(L, 1) , lua_tonumber(L, 2) , lua_tonumber(L, 3) , lua_tonumber(L, 4) ) );
#endif

	return 1;

}

//
//
//

#ifdef WICKEDENGINE
int SetEntityAttachmentVisibility (lua_State *L, bool bVisible)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int e = lua_tonumber(L, 1);
	if (e > 0 && e < t.entityelement.size())
	{
		int iGunID = t.entityelement[e].eleprof.hasweapon;
		if (iGunID > 0)
		{
			if (t.gun[iGunID].alwaysshowenemyweapon == 1) bVisible = true;
			int obj = t.entityelement[e].obj;
			if (obj > 0)
			{
				if (ObjectExist(obj) == 1 && GetVisible(obj) == 1)
				{
					int attachobj = t.entityelement[e].attachmentobj;
					if (attachobj > 0)
					{
						if (ObjectExist(attachobj) == 1)
						{
							if (bVisible == true)
								ShowObject(attachobj);
							else
								HideObject(attachobj);
						}
					}
				}
			}
		}
	}
	return 1;
}
int HideEntityAttachment (lua_State *L) { return SetEntityAttachmentVisibility(L, false); }
int ShowEntityAttachment (lua_State *L) { return SetEntityAttachmentVisibility(L, true); }
#endif

//
//
//

#ifdef WICKEDENGINE

int SetDebuggingData (lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tonumber(L, 1);
	int instructionindex = lua_tonumber(L, 2);

	extern int instruction_running_e;
	if (instruction_running_e == e)
	{
		extern int instruction_running_index;
		instruction_running_index = instructionindex;
	}

	return 1;
}

#endif

//
// New RecastDetour(RD) AI Commands
//

#ifdef WICKEDENGINE

int RDFindPath (lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 6) return 0;

	// generate path
	float fStart[3];
	fStart[0] = lua_tonumber(L, 1);
	fStart[1] = lua_tonumber(L, 2);
	fStart[2] = lua_tonumber(L, 3);
	float fEnd[3];
	fEnd[0] = lua_tonumber(L, 4);
	fEnd[1] = lua_tonumber(L, 5);
	fEnd[2] = lua_tonumber(L, 6);
	g_RecastDetour.findPath(fStart, fEnd);

	// done
	return 1;
}

int RD_GetPoint_Core (float* pXYZ,int iCurrentPoint)
{
	int iPointCount = 0;
	float* pfPointData = NULL;
	g_RecastDetour.getPath(&iPointCount, &pfPointData);
	if (iPointCount > 1 && iCurrentPoint <= iPointCount)
	{
		*(pXYZ + 0) = *(pfPointData + (iCurrentPoint * 3) + 0);
		*(pXYZ + 1) = *(pfPointData + (iCurrentPoint * 3) + 1);
		*(pXYZ + 2) = *(pfPointData + (iCurrentPoint * 3) + 2);
		return iPointCount;
	}
	return 0;
}

int RDGetPathPointCount(lua_State *L)
{
	lua = L;
	float thisPoint[3] = { 0, 0, 0 };
	int iCount = RD_GetPoint_Core(thisPoint,0);
	lua_pushnumber (L, iCount);
	return 1;
}

int RDGetPathPointX(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iPointIndex = lua_tonumber(L, 1);
	float thisPoint[3] = { 0, 0, 0 };
	RD_GetPoint_Core(thisPoint, iPointIndex);
	lua_pushnumber (L, thisPoint[0]);
	return 1;
}

int RDGetPathPointY(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iPointIndex = lua_tonumber(L, 1);
	float thisPoint[3] = { 0, 0, 0 };
	RD_GetPoint_Core(thisPoint, iPointIndex);
	lua_pushnumber (L, thisPoint[1]);
	return 1;
}

int RDGetPathPointZ(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iPointIndex = lua_tonumber(L, 1);
	float thisPoint[3] = { 0, 0, 0 };
	RD_GetPoint_Core(thisPoint, iPointIndex);
	lua_pushnumber (L, thisPoint[2]);
	return 1;
}

int StartMoveAndRotateToXYZ (lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 3 || n > 5) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		// store path on a per character basis
		float thisPoint[3] = { 0, 0, 0 };
		int iPointCount = RD_GetPoint_Core(thisPoint, 0);
		t.charanimstates[t.tcharanimindex].pathPointCount = iPointCount;
		for (int p = 0; p < iPointCount; p++)
		{
			RD_GetPoint_Core(thisPoint, p);
			t.charanimstates[t.tcharanimindex].pointx[p] = thisPoint[0];
			t.charanimstates[t.tcharanimindex].pointy[p] = thisPoint[1];
			t.charanimstates[t.tcharanimindex].pointz[p] = thisPoint[2];
		}

		// start the movement mode
		t.charanimstates[t.tcharanimindex].moveToMode = 1;
		float fMoveSpeed = lua_tonumber(L, 2);
		int movingbackward = 0; if (fMoveSpeed < 0.0f) movingbackward = 1;
		t.charanimstates[t.tcharanimindex].movingbackward = movingbackward;
		t.charanimstates[t.tcharanimindex].movespeed_f = fabs(fMoveSpeed);
		float fTurnSpeed = lua_tonumber(L, 3);
		t.charanimstates[t.tcharanimindex].turnspeed_f = fTurnSpeed;
		int iTiltMode = 0;
		if (n >= 4) iTiltMode = lua_tonumber(L, 4);
		t.charanimstates[t.tcharanimindex].iTiltMode = iTiltMode;
		int iStopFromEnd = 10;
		if (n >= 5) iStopFromEnd = lua_tonumber(L, 5);
		if (iStopFromEnd < 10) iStopFromEnd = 10;
		t.charanimstates[t.tcharanimindex].iStopFromEnd = iStopFromEnd;
	}
	return 1;
}

int MoveAndRotateToXYZ (lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 3 || n > 5) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	int iPointIndex = 0;
	float fDistanceToDest = 0.0f;
	if (t.tcharanimindex != -1)
	{
		if (t.charanimstates[t.tcharanimindex].moveToMode > 0 )
		{
			float fMoveSpeed = lua_tonumber(L, 2);
			t.charanimstates[t.tcharanimindex].movespeed_f = fabs(fMoveSpeed);
			t.charanimstates[t.tcharanimindex].turnspeed_f = lua_tonumber(L, 3);

			int iStopFromEnd = 10;
			if (n == 4) iStopFromEnd = lua_tonumber(L, 4);
			if (iStopFromEnd < 10) iStopFromEnd = 10;
			t.charanimstates[t.tcharanimindex].iStopFromEnd = iStopFromEnd;

			iPointIndex = t.charanimstates[t.tcharanimindex].moveToMode;
			fDistanceToDest = t.charanimstates[t.tcharanimindex].remainingOverallDistanceToDest_f;
		}
	}
	lua_pushnumber (L, iPointIndex);
	lua_pushnumber (L, fDistanceToDest);
	return 2;
}

int SetEntityPathRotationMode (lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		int mode = lua_tonumber(L, 2);
		t.charanimstates[t.tcharanimindex].iRotationAlongPathMode = mode;
	}
}

int RDIsWithinMesh(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	int iResult = 0;
	if (g_RecastDetour.isWithinNavMesh(fX, fY, fZ) == true)
		iResult = 1;
	else
		iResult = 0;
	lua_pushnumber (L, iResult);
	return 1;
}

int RDIsWithinAndOverMesh(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	int iResult = 0;
	float vecNearestPt[3];
	if (g_RecastDetour.isWithinNavMeshEx(fX, fY, fZ, (float*)&vecNearestPt, true) == true)
		iResult = 1;
	else
		iResult = 0;
	lua_pushnumber (L, iResult);
	return 1;
}

int RDGetYFromMeshPosition(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float thisPoint[3] = { 0, 0, 0 };
	float fNewY = g_RecastDetour.getYFromPos(fX, fY, fZ);
	// this could push object under terrain!!
	//fNewY -= 5.0f; // adjust from navmesh Y to real world Y
	// instead use result of RDGetYFromMeshPosition to then cast a proper ray to find real polygon surface!
	lua_pushnumber (L, fNewY);
	return 1;
}

int RDBlockNavMeshCore(lua_State* L,int iWithShape)
{
	// block and unblock navmesh
	lua = L;
	int n = LUA_GETTOP(L);
	if (iWithShape == 0 && n < 5) return 0;
	if (iWithShape == 1 && n < 7) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fRadius = lua_tonumber(L, 4);
	int iBlockMode = lua_tonumber(L, 5);
	float fRadius2 = fRadius;
	float fAngle = 0.0f;
	float fAdjMinY = -5.0f;
	float fAdjMaxY = 95.0f;
	if (iWithShape == 1)
	{
		fRadius2 = lua_tonumber(L, 6);
		fAngle = lua_tonumber(L, 7);
		if (n == 9)
		{
			fAdjMinY = lua_tonumber(L, 8);
			fAdjMaxY = lua_tonumber(L, 9);
		}
	}
	float thisPoint[3] = { 0, 0, 0 };
	bool bEnableBlock = false;
	if (iBlockMode != 0) bEnableBlock = true;

	// improved system for more accurate doo and navmesh blocker bounds
	g_RecastDetour.ToggleBlocker(fX, fY, fZ, fRadius, bEnableBlock, fRadius2, fAngle, fAdjMinY, fAdjMaxY);

	// go through ALL characters and request them to recalc their paths, they may no longer be valid
	int storete = t.e;
	int storecharanimindex = t.tcharanimindex;
	for (int e = 1; e <= g.entityelementmax; e++)
	{
		t.e = e; entity_lua_findcharanimstate();
		if (t.tcharanimindex != -1)
		{
			// before we trigger a path update, ensure this AIs current axtive path (if any), goes THROUGH the above blocker
			bool bThisPathWasAffected = false;
			int iPointCount = t.charanimstates[t.tcharanimindex].pathPointCount;
			int iPointIndex = t.charanimstates[t.tcharanimindex].moveToMode;
			if (iPointIndex > 0 && iPointCount > 0)
			{
				float fCurrentX = t.entityelement[e].x;
				float fCurrentZ = t.entityelement[e].z;
				bool bTradingToEndOfPath = true;
				while (bTradingToEndOfPath)
				{
					// point in path
					float thisPoint[3] = { -1, -1, -1 };
					thisPoint[0] = t.charanimstate.pointx[iPointIndex];
					thisPoint[1] = t.charanimstate.pointy[iPointIndex];
					thisPoint[2] = t.charanimstate.pointz[iPointIndex];

					// trace from current to point, does it pass through the blocker?
					float fDistX = thisPoint[0] - fCurrentX;
					float fDistZ = thisPoint[2] - fCurrentZ;
					float fDist = sqrt((fDistX * fDistX) + (fDistZ * fDistZ));
					if (fDist > 0.1f)
					{
						// step through the sliced up path line and see if at each point we entered the blocker area
						for (int iSlice = 0; iSlice < 100; iSlice++)
						{
							float fSliceX = fCurrentX + (fDistX * ((float)iSlice / 100.0f));
							float fSliceZ = fCurrentZ + (fDistZ * ((float)iSlice / 100.0f));
							if (iWithShape == 1)
							{
								// rectangle formed by fRadius as X and fRadius2 as Z and the angle
								float fRelativeToCenterOfBlockerX = fSliceX - fX;
								float fRelativeToCenterOfBlockerZ = fSliceZ - fZ;
								float fRelativeDist = sqrt(fabs(fRelativeToCenterOfBlockerX * fRelativeToCenterOfBlockerX) + fabs(fRelativeToCenterOfBlockerZ * fRelativeToCenterOfBlockerZ));
								// into the blocker's own space: it is turned by +fAngle, so the point turns by -fAngle
								float fFinalAngle = GGToDegree(atan2(fRelativeToCenterOfBlockerX, fRelativeToCenterOfBlockerZ)) - fAngle;
								fRelativeToCenterOfBlockerX = NewXValue(0, fFinalAngle, fRelativeDist);
								fRelativeToCenterOfBlockerZ = NewZValue(0, fFinalAngle, fRelativeDist);
								if (fabs(fRelativeToCenterOfBlockerX) < fRadius && fabs(fRelativeToCenterOfBlockerZ) < fRadius2)
								{
									// this point is within the blocker defined above
									if(iBlockMode!=0) bThisPathWasAffected = true;
									break;
								}
							}
							else
							{
								// simple radius check
								if ( fabs(fSliceX - fX) < fRadius && fabs(fSliceZ - fZ) < fRadius )
								{
									// this point is within blocker defined above
									if (iBlockMode != 0) bThisPathWasAffected = true;
									break;
								}
							}
						}
					}

					// move through all points to end of full path
					fCurrentX = thisPoint[0];
					fCurrentZ = thisPoint[2];
					iPointIndex++;
					if (iPointIndex >= iPointCount) bTradingToEndOfPath = false;
				}
			}
			if (bThisPathWasAffected == true)
			{
				// this triggers follow call in script to set new target (old one not valid any more after this)
				t.entityelement[e].lua.interuptpath = 5;// 50; prevent jiggling about looking for new path!
			}
		}
	}
	t.tcharanimindex = storecharanimindex;
	t.e = storete;

	// return successfully
	return 0;
}

int RDBlockNavMeshWithShape(lua_State* L)
{
	return RDBlockNavMeshCore(L,1);
}

int RDBlockNavMesh(lua_State *L)
{
	return RDBlockNavMeshCore(L,0);
}

// TokenDrop functions (include RecastDetour for convenience of navmesh rendering system)

int DoTokenDrop(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	int iType = lua_tonumber(L, 4);
	float fDuration = lua_tonumber(L, 5);
	g_RecastDetour.DoTokenDrop(fX, fY, fZ, iType, fDuration);
	return 0;
}

int GetTokenDropCount(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n > 0) return 0;
	int iTokenDropCount = g_RecastDetour.GetTokenDropCount();
	lua_pushnumber (L, iTokenDropCount);
	return 1;
}
int GetTokenDropX(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropX(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}
int GetTokenDropY(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropY(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}
int GetTokenDropZ(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropZ(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}
int GetTokenDropType(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropType(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}
int GetTokenDropTimeLeft(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropTimeLeft(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}


int AdjustPositionToGetLineOfSight (lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 8) return 0;
	int iIgnoreObjNo = lua_tonumber(L, 1);
	float fX = lua_tonumber(L, 2);
	float fY = lua_tonumber(L, 3);
	float fZ = lua_tonumber(L, 4);
	float fTargetPosX = lua_tonumber(L, 5);
	float fTargetPosY = lua_tonumber(L, 6);
	float fTargetPosZ = lua_tonumber(L, 7);
	float fRadiusOfAdjustment = lua_tonumber(L, 8);
	// project line from A to B, and if blocked, try different A positions until we have a line of sight
	// if cannot find one, return the current A position passed in and set iFoundLineOfSight to zero
	int iStaticOnly = 1;
	int iFoundLineOfSight = 0;
	int tthitvalue = 0;
	if (ODERayTerrain(fX, fY, fZ, fTargetPosX, fTargetPosY, fTargetPosZ, true) == 1) tthitvalue = -1;
	if ( tthitvalue == 0 ) tthitvalue = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fX, fY, fZ, fTargetPosX, fTargetPosY, fTargetPosZ, iIgnoreObjNo, iStaticOnly, 0, 0, 1, false);
	if ( tthitvalue != 0 )
	{
		// current A position cannot see B, try a few locations using spiral search up to adjustment radius
		float fNewX = fX;
		float fNewZ = fZ;
		float fSpiralA = 0.0f;
		float fSpiralDistance = 0.0f;
		while (fSpiralDistance < fRadiusOfAdjustment)
		{
			fSpiralA += 45.0f;
			fSpiralDistance += (25.0f / 4); //PE: Optimizing Lee test ( was (25.0f / 8) ) set at 4 so it make a circle 2 times with different distance (16 max calls).
			fNewX = fX + (cos(GGToRadian(fSpiralA))*fSpiralDistance);
			fNewZ = fZ + (sin(GGToRadian(fSpiralA))*fSpiralDistance);
			tthitvalue = 0;
			//PE: Optimizing - Snowy Mountain Stroll is getting hit by this in the tunnel. huge fps drop.
			//PE: Optimizing , this is hitting WickedCall_ SentRay3 many times (32 max calls currently) (25.0f / 8).
			if (ODERayTerrain(fNewX, fY, fNewZ, fTargetPosX, fTargetPosY, fTargetPosZ, true) == 1) tthitvalue = -1;
			if (tthitvalue == 0) tthitvalue = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fNewX, fY, fNewZ, fTargetPosX, fTargetPosY, fTargetPosZ, iIgnoreObjNo, iStaticOnly, 0, 0, 1, false);
			if (tthitvalue != 0)
			{
				// still blocked
			}
			else
			{
				// found line of sight if we shift position
				iFoundLineOfSight = 1;
				fX = fNewX;
				fZ = fNewZ;
				break;
			}
		}
	}
	else
	{
		// easy line of sight, success
		iFoundLineOfSight = 1;
	}
	lua_pushnumber (L, iFoundLineOfSight);
	lua_pushnumber (L, fX);
	lua_pushnumber (L, fZ);
	return 3;
}

int SetCharacterMode(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float e = lua_tonumber(L, 1);
	float mode = lua_tonumber(L, 2);
	int iSuccess = 0;
	if (!LuaEntityIDValid(L, (int)e, 1))
	{
		lua_pushnumber (L, iSuccess);
		return 1;
	}
	int entid = t.entityelement[e].bankindex;
	if (t.entityprofile[entid].ischaracter == 1)
	{
		// mode 0 = will disable this character so no longer handles as a character, sets object free for non-character control
		// mode 1 = will restore character as a regular character (acting as a normal ischaracter=1 object)
		t.entityelement[e].eleprof.disableascharacter = 1 - mode;
		iSuccess = 1;
	}
	lua_pushnumber (L, iSuccess);
	return 1;
}

#endif

int GetDeviceWidth(lua_State *L)
{
	lua = L;

	lua_pushinteger ( L , GetDisplayWidth() );

	return 1;
}

int GetDeviceHeight(lua_State *L)
{
	lua = L;

	lua_pushinteger ( L , GetDisplayHeight() );

	return 1;
}

int GetFirstEntitySpawn(lua_State *L)
{
	lua = L;

	int id = 0;

	if ( t.delayOneFrameForActivatedLua == 0 )
	{
		if ( t.entitiesActivatedForLua.size() > 0 )
		{
			id = t.entitiesActivatedForLua.back();
			t.entitiesActivatedForLua.pop_back();
		}
	}
	else 
		t.delayOneFrameForActivatedLua = 0;

	lua_pushinteger ( L , id );

	return 1;
}

// VR and Head Tracking

int GetHeadTracker(lua_State *L)
{
	lua = L;
	int id = 0;
	//if ( GGVR_IsHmdPresent() > 0 && g.vrglobals.GGVRUsingVRSystem == 1 ) id = 1;
	extern int g_iActivelyUsingVRNow;
	if (GGVR_IsHmdPresent() > 0 && g_iActivelyUsingVRNow == 1) id = 1;
	lua_pushinteger ( L , id );
	return 1;
}
int ResetHeadTracker(lua_State *L)
{
	lua = L;
	int id = 0;
	#ifdef VRTECH
	#else
	 SetupResetTracking();
	#endif
	lua_pushinteger ( L , id );
	return 1;
}
int GetHeadTrackerYaw(lua_State *L)
{
	lua = L;
	#ifdef VRTECH
	 float fValue = GGVR_GetHMDYaw();// + g_fDriverCompensationYaw;
	#else
	 float fValue = g_fVR920TrackingYaw + g_fDriverCompensationYaw;
	 if ( g_VR920AdapterAvailable == false ) fValue = 0.0f;
	#endif
	lua_pushnumber ( L , fValue );
	return 1;
}
int GetHeadTrackerPitch(lua_State *L)
{
	lua = L;
	#ifdef VRTECH
	 float fValue = GGVR_GetHMDPitch();// + g_fDriverCompensationYaw;
	#else
	 float fValue = g_fVR920TrackingPitch + g_fDriverCompensationPitch;
	 if ( g_VR920AdapterAvailable == false ) fValue = 0.0f;
	#endif
	lua_pushnumber ( L , fValue );
	return 1;
}
int GetHeadTrackerRoll(lua_State *L)
{
	lua = L;
	#ifdef VRTECH
	 float fValue = GGVR_GetHMDRoll();// + g_fDriverCompensationYaw;
	#else
	 float fValue = g_fVR920TrackingRoll + g_fDriverCompensationRoll;
	 if ( g_VR920AdapterAvailable == false ) fValue = 0.0f;
	#endif
	lua_pushnumber ( L , fValue );
	return 1;
}

int GetHeadTrackerNormalX(lua_State *L)
{
	lua = L;
#ifdef WICKEDENGINE
    float fValue = GGVR_GetHMDRNormalX();
#else
	float fValue = 0.0f;
#endif
	lua_pushnumber ( L , fValue );
	return 1;
}

int GetHeadTrackerNormalY(lua_State *L)
{
	lua = L;
#ifdef WICKEDENGINE
	float fValue = GGVR_GetHMDRNormalY();
#else
	float fValue = 0.0f;
#endif
	lua_pushnumber ( L , fValue );
	return 1;
}

int GetHeadTrackerNormalZ(lua_State *L)
{
	lua = L;
#ifdef WICKEDENGINE
	float fValue = GGVR_GetHMDRNormalZ();
#else
	float fValue = 0.0f;
#endif
	lua_pushnumber ( L , fValue );
	return 1;
}


// PROMPT 3D

int Prompt3D(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	char pTextToRender[1024];
	strcpy ( pTextToRender, lua_tostring(L, 1));
	DWORD dwPrompt3DTime = lua_tonumber(L, 2);
	lua_prompt3d(pTextToRender, Timer() + dwPrompt3DTime , 0 );
	return 1;
}

int PositionPrompt3D(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fAY = lua_tonumber(L, 4);
	lua_positionprompt3d(0, fX, fY, fZ, fAY, false );
	return 1;
}


int PromptLocalDuration(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	int storee = t.e;
	cstr stores = t.s_s;

	t.e = lua_tonumber(L, 1);
	const char* pStrPtr = lua_tostring(L, 2);
	if (pStrPtr)
		t.s_s = pStrPtr;
	else
		t.s_s = "";
	int addtime = lua_tonumber(L, 3);
	if (addtime < 100) addtime = 1000;

	void lua_promptlocalcore(int iTrueLocalOrForVR, int addtime = 1000);
	lua_promptlocalcore(0,addtime);

	t.e = storee;
	t.s_s = stores;

	return 0;
}



// AGK IMAGE AND SPRITE COMMANDS

int LoadImage(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, send 0 result back
	if ( n < 1 )
	{
		lua_pushnumber ( L , 0 );
		return 1;
	}

	// required image
	char filename[MAX_PATH];
	strcpy ( filename , cstr(g.fpscrootdir_s+"\\Files\\").Get());
	LPSTR pImgFile = (LPSTR)lua_tostring(L, 1);
	if (pImgFile && strlen(pImgFile) < 256) strcat (filename, pImgFile);

	// attempt a load
	int iID = GetFreeLUAImageID();
	if (iID > 0)
	{
		//LB: ensure scripts that load non existent images do not crash out!
		image_setlegacyimageloading(true);
		if (FileExist (filename) == 0)
		{
			strcpy (filename, cstr(g.fpscrootdir_s + "\\Files\\effectbank\\common\\dot.png").Get());
		}
		if (FileExist (filename) == 1)
		{
			LoadImage (filename, iID);
		}
		else
		{
			iID = 0;
		}
		image_setlegacyimageloading(false);
	}
	lua_pushnumber ( L , iID );
	return 1;
}

int GetImageWidth(lua_State *L)
{
	// get LUA param
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) { lua_pushnumber ( L , 0 ); return 1; }

	// get image width
	int iImageID = lua_tointeger(L, 1);
	float fImageWidth = ( ((float)ImageWidth(iImageID)/(float)g_dwScreenWidth) * 100.0f);

	// push return value
	lua_pushnumber ( L , fImageWidth );

	// success
	return 1;
}

int GetImageHeight(lua_State *L)
{
	// get LUA param
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) { lua_pushnumber ( L , 0 ); return 1; }

	// get image width
	int iImageID = lua_tointeger(L, 1);
	float fImageHeight = ( ((float)ImageHeight(iImageID)/(float)g_dwScreenHeight) * 100.0f);

	// push return value
	lua_pushnumber ( L , fImageHeight );

	// success
	return 1;
}

int DeleteSpriteImage(lua_State *L)
{
	// get LUA param
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) { lua_pushnumber ( L , 0 ); return 1; }

	// get image width
	int iImageID = lua_tointeger(L, 1);
	if (iImageID > 0)
	{
		if (ImageExist(iImageID) == 1)
			DeleteImage(iImageID);
	}

	// push return value
	lua_pushnumber ( L , 1 );

	// success
	return 1;
}

int CreateSprite(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, send 0 result back
	if ( n < 1 )
	{
		lua_pushnumber ( L , 0 );
		return 1;
	}

	int iID = 0;
	int iImageID = lua_tointeger(L, 1);
	if (iImageID > 0)
	{
		if (ImageExist(iImageID) == 1)
		{
			iID = GetFreeLUASpriteID();
			if (iID > 0)
			{
				Sprite (iID, 0, 0, iImageID);
				SetSpritePriority (iID, 90); // which is 10 in agk
			}
		}
	}
	lua_pushnumber ( L , iID );
	return 1;
}

int PasteSprite(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) { lua_pushnumber ( L , 0 ); return 1; }
	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		PasteSprite (iID, SpriteX(iID), SpriteY(iID));
	}
	return 0;
}

int PasteSpritePosition(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) { lua_pushnumber ( L , 0 ); return 1; }
	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		//PE: Thanks AmenMoses for this fix :)
		float fX = lua_tonumber(L, 2);
		float fY = lua_tonumber(L, 3);

		fX = (fX * g_dwScreenWidth) / 100.0f;
		fY = (fY * g_dwScreenHeight) / 100.0f;
		PasteSprite (iID, fX, fY);
	}
	return 0;
}

int SetSpriteScissor(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fW = lua_tonumber(L, 3);
	float fH = lua_tonumber(L, 4);
	fX = (fX / 100.0f) * g_dwScreenWidth;
	fY = (fY / 100.0f) * g_dwScreenHeight;
	fW = (fW / 100.0f) * g_dwScreenWidth;
	fH = (fH / 100.0f) * g_dwScreenHeight;
	ScissorSpriteArea (fX, fY, fW, fH);
	return 0;
}

int SetSpriteImage(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 2 )
		return 0;

	int iID = lua_tointeger(L, 1);
	float image = lua_tointeger(L, 2);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			Sprite (iID, SpriteX(iID), SpriteY(iID), image);
		}
	}
	return 0;
}

int SetSpritePosition(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 3 )
		return 0;

	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		float x = lua_tonumber(L, 2);
		float y = lua_tonumber(L, 3);

		x = (x * g_dwScreenWidth) / 100.0f;
		y = (y * g_dwScreenHeight) / 100.0f;

		if (iID > 0)
		{
			if (SpriteExist (iID) == 1)
			{
				Sprite (iID, x, y, GetSpriteImage(iID));
			}
		}
	}
	return 0;
}

int SetSpritePriorityForLUA(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int iID = lua_tointeger(L, 1);
	int iPriority = lua_tointeger(L, 2);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			SetSpritePriority (iID, iPriority);
		}
	}
	return 0;
}

int SetSpriteDepth(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 2 )
		return 0;

	int iID = lua_tointeger(L, 1);
	// need to flip it as agk does the order reversed
	float depth = 100 - lua_tointeger(L, 2);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			SetSpritePriority (iID, depth);
		}
	}
	return 0;
}

int SetSpriteColor(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 5 )
		return 0;

	int iID = lua_tointeger(L, 1);
	int red = lua_tointeger(L, 2);
	int green = lua_tointeger(L, 3);
	int blue = lua_tointeger(L, 4);
	int alpha = lua_tointeger(L, 5);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			SetSpriteAlpha (iID, alpha);
			SetSpriteDiffuse (iID, red, green, blue);
		}
	}
	return 0;
}

int SetSpriteAngle(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 2 )
		return 0;

	int iID = lua_tointeger(L, 1);
	int angle = lua_tonumber(L, 2);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			RotateSprite (iID, angle);
		}
	}
	return 0;
}

int DeleteSprite(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 1 )
		return 0;

	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			DeleteSprite (iID);
		}
	}
	return 0;
}

int SetSpriteOffset(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 3 )
		return 0;

	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		float x = lua_tonumber(L, 2);
		float y = lua_tonumber(L, 3);

		if (x == -1 && y == -1) return 0;

		if (x != -1)
		{
			x = (x * g_dwScreenWidth) / 100.0f;
		}

		if (y != -1)
		{
			y = (y * g_dwScreenHeight) / 100.0f;

			if (x == -1)
			{
				float perc = (y / ImageHeight(GetSpriteImage(iID))) * 100.0f;
				x = ((perc * ImageWidth(GetSpriteImage(iID))) / 100.0f) * (g_dwScreenWidth / g_dwScreenHeight);
			}
		}
		else
		{
			float perc = (x / ImageWidth(GetSpriteImage(iID))) * 100.0f;
			y = ((perc * ImageHeight(GetSpriteImage(iID))) / 100.0f) * (g_dwScreenWidth / g_dwScreenHeight);
		}

		if (SpriteExist (iID) == 1)
		{
			OffsetSprite (iID, x, y);
		}
	}
	return 0;
}

int SetSpriteSize ( lua_State *L )
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 3 )
		return 0;

	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		float sizeX = lua_tonumber(L, 2);
		float sizeY = lua_tonumber(L, 3);

		//PE: vertex data use iXOffset)+iWidth-0.5f, (-0.5f) soo add a bit.
		//PE: mainly visible when using 100 percent
		//PE: https://github.com/TheGameCreators/GameGuruRepo/issues/423
		//PE: So 100 percent do not fill the entire screen.
		//PE: Did not want to change vertex data as it might change how sprites display on other level.
		//PE: So for now just do this to fill the hole screen. (should be changed in vertex at some point)

		if (sizeX == 100) {
			sizeX += 0.1;
		}
		if (sizeY == 100) {
			sizeY += 0.1;
		}

		if (sizeX == -1 && sizeY == -1) return 0;

		if (sizeX != -1)
		{
			sizeX = (sizeX * g_dwScreenWidth) / 100.0f;
		}

		if (sizeY != -1)
		{
			sizeY = (sizeY * g_dwScreenHeight) / 100.0f;

			if (sizeX == -1)
			{
				float perc = (sizeY / ImageHeight(GetSpriteImage(iID))) * 100.0f;
				sizeX = ((perc * ImageWidth(GetSpriteImage(iID))) / 100.0f) * (g_dwScreenWidth / g_dwScreenHeight);
			}
		}
		else
		{
			//PE: 11-06-19 issue: https://github.com/TheGameCreators/GameGuruRepo/issues/504
			//PE: I cant test this on my system , but assume we could always use the backbuffer size g_pGlob->iScreenWidth instead of the screenwidth g_dwScreenWidth
			//PE: Can someone with a similar screen setup do this, test the return of these 2 MessageBox.
			//PE: They should always be the same , but issue indicate they are not.
			//PE: Just add rader.lua and enable the below 4 lines to test :)
	//		char tmp[80]; sprintf(tmp, "g_pGlob->iScreenWidth: %d", g_pGlob->iScreenWidth); // 1920
	//		MessageBox(NULL, tmp, "g_pGlob->iScreenWidth", MB_TOPMOST | MB_OK);
	//		sprintf(tmp, "g_dwScreenWidth: %d", g_dwScreenWidth); // 1920
	//		MessageBox(NULL, tmp, "g_dwScreenWidth", MB_TOPMOST | MB_OK);
			float perc = (sizeX / ImageWidth(GetSpriteImage(iID))) * 100.0f;
			sizeY = ((perc * ImageHeight(GetSpriteImage(iID))) / 100.0f) * (g_dwScreenWidth / g_dwScreenHeight);
		}

		if (SpriteExist (iID) == 1)
		{
			SizeSprite (iID, sizeX, sizeY);
		}
	}
	return 0;
}

int DrawSpritesFirstForLUA ( lua_State *L )
{
	DrawSpritesFirst();
	return 0;
}

int DrawSpritesLastForLUA ( lua_State *L )
{
	DrawSpritesLast();
	return 0;
}

int BackdropOffForLUA ( lua_State *L )
{
	BackdropOff();
	return 0;
}

int BackdropOnForLUA ( lua_State *L )
{
	BackdropOn();
	return 0;
}

int LoadGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	const char* pFilename = lua_tostring(L, 1);
	int iID = g.globalsoundoffset + lua_tointeger(L, 2);
	if ( SoundExist(iID)==1 ) DeleteSound(iID);
	LoadSound((LPSTR)pFilename,iID);
	return 0;
}
int PlayGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	if ( SoundExist(iID)==1 )
	{
		PlaySound(iID);
	}
	return 0;
}
int LoopGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	if ( SoundExist(iID)==1 )
	{
		LoopSound(iID);
	}
	return 0;
}
int StopGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	if ( SoundExist(iID)==1 )
	{
		StopSound(iID);
	}
	return 0;
}
int DeleteGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	if ( SoundExist(iID)==1 )
	{
		DeleteSound(iID);
	}
	return 0;
}
int SetGlobalSoundSpeed ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	int iSpeed = lua_tointeger(L, 2);
	if ( SoundExist(iID)==1 )
	{
		SetSoundSpeed(iID,iSpeed);
	}
	return 0;
}
int SetGlobalSoundVolume ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	int iVolume = lua_tointeger(L, 2);
	if ( SoundExist(iID)==1 )
	{
		SetSoundVolume(iID,iVolume);
		extern std::unordered_map<int, float> luavolumes;
		luavolumes.insert_or_assign(iID, iVolume);
	}
	return 0;
}
int GetGlobalSoundExist(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	lua_pushinteger ( L , SoundExist ( iID ) );
	return 1;
}
int GetGlobalSoundPlaying(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	lua_pushinteger ( L , SoundPlaying ( iID ) );
	return 1;
}
int GetGlobalSoundLooping(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	lua_pushinteger ( L , SoundLooping ( iID ) );
	return 1;
}

int GetSoundPlaying(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tointeger(L, 1);
	int v = lua_tointeger(L, 2);
	int tsnd = 0;
	if (!LuaEntityIDValid(L, e, 1))
	{
		lua_pushinteger (L, 0);
		return 1;
	}
	if (v == 0) tsnd = t.entityelement[e].soundset;
	if (v == 1) tsnd = t.entityelement[e].soundset1;
	if (v == 2) tsnd = t.entityelement[e].soundset2;
	if (v == 3) tsnd = t.entityelement[e].soundset3;
	if (v == 4) tsnd = t.entityelement[e].soundset4;
	if (v == 5) tsnd = t.entityelement[e].soundset5;
	if (v == 6) tsnd = t.entityelement[e].soundset6;
	int iPlaying = 0;
	if (tsnd > 0)
	{
		if (SoundExist(tsnd) == 1 && SoundPlaying(tsnd) == 1)
		{
			iPlaying = tsnd;
		}
	}
	lua_pushinteger (L, iPlaying);
	return 1;
}

int SetRawSoundData ( lua_State *L, int iDataMode )
{
	lua = L;
	int iParamNum = 0;
	switch ( iDataMode )
	{
		case 1 : iParamNum = 1;	break;
		case 2 : iParamNum = 1;	break;
		case 3 : iParamNum = 1;	break;
		case 4 : iParamNum = 2;	break;
		case 5 : iParamNum = 2;	break;
		case 6 : iParamNum = 2;	break;
	}
	int n = LUA_GETTOP(L);
	if ( n < iParamNum ) return 0;
	int iSoundID = lua_tonumber(L, 1);
	if (iSoundID > 0 && SoundExist(iSoundID) == 1)
	{
		switch (iDataMode)
		{
		case 1: PlaySound(lua_tonumber(L, 1)); break;
		case 2: LoopSound(lua_tonumber(L, 1)); break;
		case 3: StopSound(lua_tonumber(L, 1)); break;
		case 4:
		{
			int sndid = lua_tonumber(L, 1);
			float volume = soundtruevolume(lua_tonumber(L, 2));
			SetSoundVolume(sndid, volume);
			extern std::unordered_map<int, float> luavolumes;
			luavolumes.insert_or_assign(sndid, volume);
			break;
		}
		case 5: SetSoundSpeed(lua_tonumber(L, 1), lua_tonumber(L, 2)); break;
		case 6: SetSoundDistanceScale(lua_tonumber(L, 1), lua_tonumber(L, 2)); break;
		}
	}
	return 0;
}
int GetRawSoundData ( lua_State *L, int iDataMode )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	switch ( iDataMode )
	{
		case 1 : lua_pushnumber ( L , SoundExist ( lua_tonumber(L, 1) ) ); break;
		case 2 : lua_pushnumber ( L , SoundPlaying ( lua_tonumber(L, 1) ) ); break;
		case 3 : lua_pushnumber ( L , t.entityelement[lua_tonumber(L, 1)].soundset ); break;
		case 4 : lua_pushnumber ( L , GetSoundDistanceScale ( lua_tonumber(L, 1) ) ); break;
	}
	return 1;
}
int PlayRawSound ( lua_State *L ) { return SetRawSoundData ( L, 1 ); }
int LoopRawSound ( lua_State *L ) { return SetRawSoundData ( L, 2 ); }
int StopRawSound ( lua_State *L ) { return SetRawSoundData ( L, 3 ); }
int SetRawSoundVolume ( lua_State *L ) { return SetRawSoundData ( L, 4 ); }
int SetRawSoundSpeed ( lua_State *L ) { return SetRawSoundData ( L, 5 ); }
int SetRawSoundDistanceScale ( lua_State *L ) { return SetRawSoundData ( L, 6 ); }
int GetRawSoundDistanceScale ( lua_State *L ) { return GetRawSoundData ( L, 4 ); }
int RawSoundExist ( lua_State *L ) { return GetRawSoundData ( L, 1 ); }
int RawSoundPlaying ( lua_State *L ) { return GetRawSoundData ( L, 2 ); }

// SetDopplerScale(scale): how strongly 3D sounds rise in pitch as they come closer and fall as they go away, from how fast
// each sound and the camera move; 1 as in the real world (the default), 0 for none, more to exaggerate. Back to 1 at the
// next level
int SetDopplerScale ( lua_State *L )
{
	lua = L;
	if ( LUA_GETTOP(L) < 1 ) return 0;
	wiAudio::SetDopplerScale ( lua_tonumber(L, 1) );
	return 0;
}
int GetDopplerScale ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, wiAudio::GetDopplerScale() );
	return 1;
}

int GetEntityRawSound(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iE = lua_tonumber(L, 1);
	int iSoundSlot = lua_tonumber(L, 2);
	int iRawSoundIndex = 0;
	if (!LuaEntityIDValid(L, iE, 1))
	{
		lua_pushnumber ( L , iRawSoundIndex );
		return 1;
	}
	if (iSoundSlot == 0) iRawSoundIndex = t.entityelement[iE].soundset;
	if (iSoundSlot == 1) iRawSoundIndex = t.entityelement[iE].soundset1;
	if (iSoundSlot == 2) iRawSoundIndex = t.entityelement[iE].soundset2;
	if (iSoundSlot == 3) iRawSoundIndex = t.entityelement[iE].soundset3;
	if (iSoundSlot == 4) iRawSoundIndex = t.entityelement[iE].soundset4;
	if (iSoundSlot == 5) iRawSoundIndex = t.entityelement[iE].soundset5;
	if (iSoundSlot == 6) iRawSoundIndex = t.entityelement[iE].soundset6;
	lua_pushnumber ( L , iRawSoundIndex );
	return 1;
}

#ifdef WICKEDENGINE
int StartAmbientMusicTrack(lua_State* L)
{
	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (FileExist(t.visuals.sAmbientMusicTrack.Get()) == 1)
		{
			if (SoundExist(iFreeSoundID) == 1)
			{
				LoopSound(iFreeSoundID);
				SetSoundVolume(iFreeSoundID, t.visuals.iAmbientMusicTrackVolume);
				extern std::unordered_map<int, float> luavolumes;
				luavolumes.insert_or_assign(iFreeSoundID, t.visuals.iAmbientMusicTrackVolume);
			}
		}
	}
	return 0;
}
int StopAmbientMusicTrack(lua_State* L)
{
	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
		{
			StopSound(iFreeSoundID);
		}
	}
	return 0;
}
int SetAmbientMusicTrackVolume(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (SoundExist(iFreeSoundID) == 1)
		{
			float fVolumePercentage = lua_tonumber(L, 1) / 100.0f;
			float fFinalVolume = t.visuals.iAmbientMusicTrackVolume * fVolumePercentage;
			SetSoundVolume(iFreeSoundID, fFinalVolume);
			extern std::unordered_map<int, float> luavolumes;
			luavolumes.insert_or_assign(iFreeSoundID, fFinalVolume);
		}
	}
	return 0;
}
int StartCombatMusicTrack(lua_State* L)
{
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (FileExist(t.visuals.sCombatMusicTrack.Get()) == 1)
	{
		if (SoundExist(iFreeSoundID) == 1)
		{
			LoopSound(iFreeSoundID);
			SetSoundVolume(iFreeSoundID, t.visuals.iCombatMusicTrackVolume);
			extern std::unordered_map<int, float> luavolumes;
			luavolumes.insert_or_assign(iFreeSoundID, t.visuals.iCombatMusicTrackVolume);
		}
	}
	return 0;
}
int StopCombatMusicTrack(lua_State* L)
{
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
	{
		StopSound(iFreeSoundID);
	}
	return 0;
}
int SetCombatMusicTrackVolume(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1)
	{
		float fVolumePercentage = lua_tonumber(L, 1) / 100.0f;
		float fFinalVolume = t.visuals.iCombatMusicTrackVolume * fVolumePercentage;
		SetSoundVolume(iFreeSoundID, fFinalVolume);
		extern std::unordered_map<int, float> luavolumes;
		luavolumes.insert_or_assign(iFreeSoundID, fFinalVolume);
	}
	return 0;
}
int GetCombatMusicTrackPlaying(lua_State *L)
{
	lua = L;
	int iPlaying = 0;
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
	{
		iPlaying = 1;
	}
	lua_pushnumber (L, iPlaying);
	return 1;
}

int SetSoundMusicMode(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	extern bool g_bSoundIsMusic[65536];
	int iSoundIndex = lua_tonumber(L, 1);
	g_bSoundIsMusic[iSoundIndex] = lua_tonumber(L, 2);
	//audio_volume_update(); this is a MASSIVE PERF HIT FOR LEVELS THAT USE LOTS OF SOUNDS
	return 1;
}
int GetSoundMusicMode(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	extern bool g_bSoundIsMusic[65536];
	int iSoundIndex = lua_tonumber(L, 1);
	lua_pushnumber (L, g_bSoundIsMusic[iSoundIndex]);
	return 1;
}

#endif

// Voice

#ifdef VRTECH
int GetSpeech(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iRunning = 0;
	int iE = lua_tonumber(L, 1);
	int iCharAnimIndex = -1;
	for ( int tcharanimindex = 1 ; tcharanimindex <= g.charanimindexmax; tcharanimindex++ )
	{
		if (  t.charanimstates[tcharanimindex].e == iE ) { iCharAnimIndex = tcharanimindex  ; break; }
	}
	if ( iCharAnimIndex != -1 )
	{
		if ( t.charanimstates[iCharAnimIndex].ccpo.speak.fMouthTimeStamp > 0.0f ) iRunning = 1;
	}
	lua_pushinteger ( L, iRunning );
	return 1;
}
#endif

// Generic

int GetTimeElapsed ( lua_State *L )
{
	lua = L;
	extern bool g_bGamePaused;
	if ( g_bGamePaused == true )
		lua_pushnumber ( L, 0.0f );
	else
		lua_pushnumber ( L, g.timeelapsed_f );
	return 1;
}

int GetKeyState ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iKeyValue = lua_tonumber(L, 1);
	lua_pushnumber ( L, KeyState(g.keymap[iKeyValue]) );
	return 1;
}

int SetGlobalTimer (lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iRestoreToTime = lua_tonumber(L, 1);
	extern DWORD g_dwAppLocalTimeStart;
	extern void SetLocalTimerReset(void);
	SetLocalTimerReset();
	g_dwAppLocalTimeStart -= iRestoreToTime;
	return 1;
}

int GetGlobalTimer ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, Timer() );
	return 1;
}

int MouseMoveX ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, MouseMoveX() );
	return 1;
}
int MouseMoveY ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, MouseMoveY() );
	return 1;
}
int GetDesktopWidth ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, GetDesktopWidth() );
	return 1;
}
int GetDesktopHeight ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, GetDesktopHeight() );
	return 1;
}
int CurveValue ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float a = lua_tonumber(L, 1);
	float b = lua_tonumber(L, 2);
	float c = lua_tonumber(L, 3);
	lua_pushnumber ( L, CurveValue(a, b, c) );
	return 1;
}
int CurveAngle ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float a = lua_tonumber(L, 1);
	float b = lua_tonumber(L, 2);
	float c = lua_tonumber(L, 3);
	lua_pushnumber ( L, CurveAngle(a, b, c) );
	return 1;
}
int PositionMouse ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	float fScreenX = lua_tonumber(L, 1);
	float fScreenY = lua_tonumber(L, 2);
	PositionMouse ( fScreenX, fScreenY );
	g.LUAMouseX = fScreenX;
	g.LUAMouseY = fScreenY;
	return 0;
}

int GetDynamicCharacterControllerDidJump ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, ODEGetDynamicCharacterControllerDidJump() );
	return 1;
}
int GetCharacterControllerDucking ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, ODEGetCharacterControllerDucking(t.aisystem.objectstartindex) );
	return 1;
}

int WrapValue ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	float a = lua_tonumber(L, 1);
	lua_pushnumber ( L, WrapValue(a) );
	return 1;
}
int GetElapsedTime ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, t.ElapsedTime_f );
	return 1;
}
// SetGamePaused(on): 1 pauses the game world for a menu as the in-game menu does (physics, the player, entities and their
// animation, AI, weapons, projectiles, particles and the game's sounds) while every script, the HUD, sprites, music and
// rendering carry on, and new sounds play; g_Time holds still and g_TimeElapsed and GetTimeElapsed() read 0 (Timer() and
// GetElapsedTime() are the real clock). 0 resumes. A level that ends resumes it
int SetGamePaused ( lua_State *L )
{
	lua = L;
	if ( LUA_GETTOP(L) < 1 ) return 0;
	extern void game_setgamepaused ( bool bPause );
	game_setgamepaused ( lua_tonumber(L, 1) != 0 );
	return 0;
}
// SetEscapeMenu(on): 0 leaves Escape (and a controller's START) to the scripts, for a menu of their own: it no longer
// opens the in-game menu, nor leaves a test level, though shift and Escape still do that. 1 is the usual; back to 1 at
// each level start
int SetEscapeMenu ( lua_State *L )
{
	lua = L;
	if ( LUA_GETTOP(L) < 1 ) return 0;
	extern bool g_bEscapeMenu;
	g_bEscapeMenu = lua_tonumber(L, 1) != 0;
	return 0;
}
// GetGamePaused(): 1 while SetGamePaused holds the world
int GetGamePaused ( lua_State *L )
{
	lua = L;
	extern bool g_bGamePaused;
	lua_pushnumber ( L, g_bGamePaused == true ? 1 : 0 );
	return 1;
}
// GetVideoMemoryUsed(): the video memory the game uses on its graphics card and the budget Windows gives it there, both
// in MB (free is budget - used), then the shared system memory the card uses and its budget. The shared figure grows when
// the dedicated memory nears its budget and Windows moves resources out, which is slow: the early warning of running out
int GetVideoMemoryUsed ( lua_State *L )
{
	lua = L;
	float fUsedMB = 0, fBudgetMB = 0, fSharedUsedMB = 0, fSharedBudgetMB = 0;
	extern void GetVramUsageAndBudget ( float* pfUsedMB, float* pfBudgetMB, float* pfSharedUsedMB, float* pfSharedBudgetMB );
	GetVramUsageAndBudget ( &fUsedMB, &fBudgetMB, &fSharedUsedMB, &fSharedBudgetMB );
	lua_pushnumber ( L, fUsedMB );
	lua_pushnumber ( L, fBudgetMB );
	lua_pushnumber ( L, fSharedUsedMB );
	lua_pushnumber ( L, fSharedBudgetMB );
	return 4;
}
// GetDrawCalls(): the last frame's draw calls in the main pass, in the shadow passes and for transparent objects, and
// the triangles the main pass drew
int GetDrawCalls ( lua_State *L )
{
	lua = L;
	lua_pushinteger ( L, wiProfiler::GetDrawCalls() );
	lua_pushinteger ( L, wiProfiler::GetDrawCallsShadows() );
	lua_pushinteger ( L, wiProfiler::GetDrawCallsTransparent() );
	lua_pushinteger ( L, wiProfiler::GetPolygons() );
	return 4;
}
// SetRenderTiming(on): 1 times the renderer's work every frame (the ranges of Tab Tab's performance data) so GetRenderTime
// can read them, 0 stops. It costs a few GPU timestamp queries a frame while on, and is off again at the next level
int SetRenderTiming ( lua_State *L )
{
	lua = L;
	if ( LUA_GETTOP(L) < 1 ) return 0;
	extern bool g_bLuaRenderTiming;
	g_bLuaRenderTiming = ( lua_tointeger(L, 1) != 0 );
	wiProfiler::SetEnabled ( g_bLuaRenderTiming );
	return 0;
}
// GetRenderTime([name]): milliseconds, averaged over the last 20 frames, of a timed range: "GPU Frame" (the default, the GPU's
// time for a whole frame), "CPU Frame", or another range named in Tab Tab's performance data. -1 while SetRenderTiming is
// off and for the first frames after it is turned on. The second value is how many frames the time has gone without a new
// GPU result (0 when fresh), as the GPU's results arrive late when it runs far behind
int GetRenderTime ( lua_State *L )
{
	lua = L;
	const char* pName = "GPU Frame";
	if ( LUA_GETTOP(L) >= 1 && lua_isstring(L, 1) ) pName = lua_tostring(L, 1);
	int iStaleFrames = 0;
	lua_pushnumber ( L, wiProfiler::GetRangeTime ( pName, &iStaleFrames ) );
	lua_pushinteger ( L, iStaleFrames );
	return 2;
}
int GetPlrObjectPositionX ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, ObjectPositionX(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectPositionY ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, ObjectPositionY(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectPositionZ ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, ObjectPositionZ(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectAngleX ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, ObjectAngleX(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectAngleY ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, ObjectAngleY(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectAngleZ ( lua_State *L )
{
	lua = L;
	lua_pushnumber ( L, ObjectAngleZ(t.aisystem.objectstartindex) );
	return 1;
}
int GetGroundHeight ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	float x = lua_tonumber(L, 1);
	float z = lua_tonumber(L, 2);
	lua_pushnumber ( L, BT_GetGroundHeight(t.terrain.TerrainID,x,z) );
	return 1;
}
int NewXValue ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float a = lua_tonumber(L, 1);
	float b = lua_tonumber(L, 2);
	float c = lua_tonumber(L, 3);
	lua_pushnumber ( L, NewXValue(a, b, c) );
	return 1;
}
int NewZValue ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float a = lua_tonumber(L, 1);
	float b = lua_tonumber(L, 2);
	float c = lua_tonumber(L, 3);
	lua_pushnumber ( L, NewZValue(a, b, c) );
	return 1;
}

int ControlDynamicCharacterController ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 8 ) return 0;
	float fAngleY = lua_tonumber(L, 1);
	float fAngleX = lua_tonumber(L, 2);
	float fSpeed = lua_tonumber(L, 3);
	float fJump = lua_tonumber(L, 4);
	float fDucking = lua_tonumber(L, 5);
	float fPushAngle = lua_tonumber(L, 6);
	float fPushForce = lua_tonumber(L, 7);
	float fThrustUpwards = lua_tonumber(L, 8);

	#ifdef VRTECH
	if ( g.vrglobals.GGVREnabled == 0 )
	{
		// no VR control
		ODESetDynamicCharacterController (t.aisystem.objectstartindex, t.terrain.waterliney_f, 0, 0, 0, 0, 0, 0);
		ODEControlDynamicCharacterController ( t.aisystem.objectstartindex, fAngleY, fAngleX, fSpeed, fJump, fDucking, fPushAngle, fPushForce, fThrustUpwards );
	}
	else
	{
		// VR Control (standing or seated)
		double Modified_fAngleY = 0.0;
		double norm = 0.0;
		if ( g.vrglobals.GGVRStandingMode == 1 )
		{
			// VR Controlled player capsule
			double norm_XOffset = 0.0;
			double norm_ZOffset = 0.0;

			// Rotate the offset around the Yaw of the HMD to make it relative to the HMD facing
			double radian = 0.0174532988888;
			double modifiedX = 0.0;
			double modifiedZ = 0.0;
			double HMDYaw = GGVR_GetHMDYaw();
			double camL = t.playercontrol.finalcameraangley_f;

			if (HMDYaw < 0.0)
			{
				HMDYaw = 360.0 + HMDYaw;
			}
			if (HMDYaw > 360.0)
			{
				HMDYaw = HMDYaw - 360.0;
			}
			if (camL < 0.0)
			{
				camL = 360.0 + camL;
			}
			if (camL > 360.0)
			{
				camL = camL - 360.0;
			}

			double yl = (camL - HMDYaw);

			if (yl < 0.0f)
			{
				yl = 360.0f + yl;
			}
			if (yl > 360.0f)
			{
				yl = yl - 360.0f;
			}
		
			yl = yl *  radian;
			double cosYL = cos(yl);  double sinYL = sin(yl); double nsinYL = -sin(yl);

			// move player along X and Z if in standing mode
			modifiedX = (sin(fAngleY*radian)*fSpeed) + ((g.vrglobals.GGVR_XposOffsetChange*cosYL) + (g.vrglobals.GGVR_ZposOffsetChange*sinYL));
			modifiedZ = (cos(fAngleY*radian)*fSpeed) + ((g.vrglobals.GGVR_XposOffsetChange*nsinYL) + (g.vrglobals.GGVR_ZposOffsetChange*cosYL));
		
			norm_XOffset = 0.0;
			norm_ZOffset = 0.0;
			Modified_fAngleY = 0.0;
		
			//Work out the motion angle of the HMD in the play area
			norm = sqrt((modifiedX*modifiedX) + (modifiedZ*modifiedZ));
			if (norm != 0.0)
			{
				double XOffset = modifiedX / norm;
				double ZOffset = modifiedZ / norm;
				double MovementAngle = 0.0f;

				if (XOffset == 0.0)
				{
					if (ZOffset > 0.0f)
					{
						MovementAngle = 0.0f;
					}
					else
					{
						MovementAngle = 180.0f;
					}
				}
				if (XOffset > 0.0)
				{
					if (ZOffset >= 0.0f)
					{
						MovementAngle = Asin(XOffset);
					}
					else
					{
						MovementAngle = 180.0f - Asin(XOffset);
					}
				}
				if (XOffset < 0.0)
				{
					if (ZOffset >= 0.0)
					{
						MovementAngle = 360.0f + Asin(XOffset);
					}
					else
					{
						MovementAngle = 180.0f - Asin(XOffset);
					}
				}

				Modified_fAngleY = MovementAngle;

			}
			else
			{
				norm_XOffset = 0.0;
				norm_ZOffset = 0.0;
				norm = 0.0;
				Modified_fAngleY = fAngleY;
			}
		}
		else
		{
			norm = fSpeed;
			Modified_fAngleY = fAngleY;
		}
		ODESetDynamicCharacterController (t.aisystem.objectstartindex, t.terrain.waterliney_f, 0, 0, 0, 0, 0, 0);
		ODEControlDynamicCharacterController(t.aisystem.objectstartindex, Modified_fAngleY, fAngleX, norm, fJump, fDucking, fPushAngle, fPushForce, fThrustUpwards);
	}
	#else
		// no VR control
		ODESetDynamicCharacterController (t.aisystem.objectstartindex, t.terrain.waterliney_f, 0, 0, 0, 0, 0, 0);
		ODEControlDynamicCharacterController ( t.aisystem.objectstartindex, fAngleY, fAngleX, fSpeed, fJump, fDucking, fPushAngle, fPushForce, fThrustUpwards );
	#endif
	return 0;
}

int SetCharacterDirectionOverride(lua_State* L)
{
	// Check for the correct parameter count.
	int n = LUA_GETTOP(L);
	if (n < 7) 
		return 0;

	// Extract parameter values.
	float fAngleX = lua_tonumber(L, 1);
	float fAngleY = lua_tonumber(L, 2);
	float fAngleZ = lua_tonumber(L, 3);
	float fAxisX = lua_tonumber(L, 4);
	float fAxisY = lua_tonumber(L, 5);
	float fAxisZ = lua_tonumber(L, 6);
	float fMagnitude = lua_tonumber(L, 7);
	
	ODESetCharacterDirectionOverride(fAngleX, fAngleY, fAngleZ, fAxisX, fAxisY, fAxisZ, fMagnitude);

	return 1;
}

int LimitSwimmingVerticalMovement(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1)
		return 0;

	int value = lua_tonumber(L, 1);
	ODESwimmingLimitVertical(value);
}


int GetCharacterHitFloor ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetCharacterHitFloor() );
	return 1;
}
int GetCharacterFallDistance ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetCharacterFallDistance() );
	return 1;
}
int RayTerrain ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fToX = lua_tonumber(L, 4);
	float fToY = lua_tonumber(L, 5);
	float fToZ = lua_tonumber(L, 6);
	lua_pushnumber ( L, ODERayTerrain(fX, fY, fZ, fToX, fToY, fToZ, true) );
	return 1;
}
int GetRayCollisionX ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayCollisionX() );
	return 1;
}
int GetRayCollisionY ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayCollisionY() );
	return 1;
}
int GetRayCollisionZ ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayCollisionZ() );
	return 1;
}
// GetRayNormalX/Y/Z(): the surface normal where the last RayTerrain hit, the ground or a tree trunk
int GetRayNormalX ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayNormalX() );
	return 1;
}
int GetRayNormalY ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayNormalY() );
	return 1;
}
int GetRayNormalZ ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayNormalZ() );
	return 1;
}

// the ignore argument of the intersect commands: an object number, or a table of object numbers
static void LuaReadIntersectIgnore(lua_State* L, int iArg, std::vector<int>& ignore)
{
	if (lua_istable(L, iArg))
	{
		for (int i = 1; i <= 256; i++)
		{
			lua_rawgeti(L, iArg, i);
			bool bEnd = lua_isnil(L, -1);
			int iObj = bEnd ? 0 : (int)lua_tonumber(L, -1);
			lua_pop(L, 1);
			if (bEnd) break;
			if (iObj > 0) ignore.push_back(iObj);
		}
	}
	else
	{
		int iObj = lua_tonumber(L, iArg);
		if (iObj > 0) ignore.push_back(iObj);
	}
}

int IntersectCore (lua_State* L, int iMode)
{
	#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
	#endif
	// iMode : 0=dynamic, 1=staticonly, 2-performant, 3-dynamic and use terrain hit to adjust ray to detect objects only
	int n = LUA_GETTOP(L);
	if (iMode == 2)
	{
		if (n < 9) return 0;
	}
	else
	{
		if (n < 7) return 0;
	}
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fNewX = lua_tonumber(L, 4);
	float fNewY = lua_tonumber(L, 5);
	float fNewZ = lua_tonumber(L, 6);

	// the object to ignore, or a table of them: the first goes to IntersectAllEx, the rest to its extra ignore list
	std::vector<int> ignore;
	LuaReadIntersectIgnore(L, 7, ignore);
	int iIgnoreObjNo = 0;
	if (ignore.size() > 0) iIgnoreObjNo = ignore[0];

	// use a database to store recent results, and pull from that before redoing a real intersect test
	int iIndexInIntersectDatabase = 0;
	int iLifeInMilliseconds = 0;
	int iIgnorePlayerCapsule = 0;
	int iIgnoreTerrain = 0;
	if (iMode == 0 && n == 7)
	{
		//PE: Now that terrain ray is working all scripts that use IntersectAll now hits terrain. Ignore terrain so it works like before.
		//PE: IntersectAll can now be set to include terrain parameter 11 , for new scripts.
		iIgnoreTerrain = 1;
	}
	if (iMode == 2)
	{
		iIndexInIntersectDatabase = lua_tonumber(L, 8);
		iLifeInMilliseconds = lua_tonumber(L, 9);
		if (n < 10)
			iIgnorePlayerCapsule = 1;
		else
			iIgnorePlayerCapsule = lua_tonumber(L, 10);
		if (n < 11)
			iIgnoreTerrain = 0;
		else
			iIgnoreTerrain = lua_tonumber(L, 11);
	}

	// catch silly tests
	if ((fX == 0 && fY == 0 && fZ == 0 && fNewX == 0 && fNewY == 0 && fNewZ == 0) || (fX == fNewX && fY == fNewY && fZ == fNewZ))
	{
		if (iMode == 2 && iLifeInMilliseconds == -1)
		{
			// resetting the database is not a silly test, it is a signal!
		}
		else
		{
			lua_pushnumber (L, 0);
			return 1;
		}
	}

	// do the expensive ray cast
	extern std::vector<int> g_IntersectExtraIgnoreObjects;
	g_IntersectExtraIgnoreObjects.clear();
	for (int i = 1; i < (int)ignore.size(); i++) g_IntersectExtraIgnoreObjects.push_back(ignore[i]);
	int tthitvalue = 0;
	if ( iIgnoreTerrain == 0 && iLifeInMilliseconds != -1 && ODERayTerrain(fX, fY, fZ, fNewX, fNewY, fNewZ, true) == 1)
	{
		// dynamic and use terrain hit to adjust ray to detect objects only
		if (iMode == 3)
		{
			fNewX = ODEGetRayCollisionX();
			fNewY = ODEGetRayCollisionY();
			fNewZ = ODEGetRayCollisionZ();
		}
		else
		{
			tthitvalue = -1;

			// the entity test below never ran on a terrain hit, so an entity in front of the ground was missed and the
			// hit point was never stored. ODERayTerrain reports the nearer of the ground and a tree trunk, so the ray is
			// shortened to that point to look for a nearer entity (modes 0 and 1; the performant mode 2 skips that
			// extra pick). Either point is stored
			float fGroundX = ODEGetRayCollisionX();
			float fGroundY = ODEGetRayCollisionY();
			float fGroundZ = ODEGetRayCollisionZ();
			float fGroundNX = ODEGetRayNormalX();
			float fGroundNY = ODEGetRayNormalY();
			float fGroundNZ = ODEGetRayNormalZ();
			int iNearerHit = 0;
			if (iMode == 0 || iMode == 1)
			{
				iNearerHit = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fX, fY, fZ, fGroundX, fGroundY, fGroundZ, iIgnoreObjNo, iMode, iIndexInIntersectDatabase, iLifeInMilliseconds, iIgnorePlayerCapsule, true);
			}
			if (iNearerHit > 0)
			{
				tthitvalue = iNearerHit;
			}
			else
			{
				g_pGlob->checklist[5].fvaluea = fGroundX;
				g_pGlob->checklist[5].fvalueb = fGroundY;
				g_pGlob->checklist[5].fvaluec = fGroundZ;
				g_pGlob->checklist[6].fvaluea = fGroundNX;
				g_pGlob->checklist[6].fvalueb = fGroundNY;
				g_pGlob->checklist[6].fvaluec = fGroundNZ;
				g_pGlob->checklist[0].valueb = -1; // no limb on terrain (GetIntersectCollisionLimb)
				g_pGlob->checklistqty = 7;
			}
		}
	}
	bool bFullWickedAccuracy = true;
	if (iMode == 2) bFullWickedAccuracy = false;
	if (tthitvalue == 0 ) tthitvalue = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fX, fY, fZ, fNewX, fNewY, fNewZ, iIgnoreObjNo, iMode, iIndexInIntersectDatabase, iLifeInMilliseconds, iIgnorePlayerCapsule, bFullWickedAccuracy);
	g_IntersectExtraIgnoreObjects.clear();
	lua_pushnumber ( L, tthitvalue );
	return 1;
}

// IntersectRay(x1, y1, z1, x2, y2, z2 [, ignore [, flags]]): one full-accuracy pick with all its results. ignore is an
// object number or a table of them. flags add up: 1 also hits terrain and tree trunks, 2 passes through
// entities whose collisionmode is 11, as the player's bullets do. Returns obj (0 none, -1 terrain or other geometry),
// e (0 if not an entity), x, y, z, nx, ny, nz (the end of the ray and 0,0,0 when nothing was hit), limb (-1 none)
int IntersectRay ( lua_State* L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	float fX = lua_tonumber(L, 1), fY = lua_tonumber(L, 2), fZ = lua_tonumber(L, 3);
	float fEndX = lua_tonumber(L, 4), fEndY = lua_tonumber(L, 5), fEndZ = lua_tonumber(L, 6);
	std::vector<int> ignore;
	if ( n >= 7 ) LuaReadIntersectIgnore(L, 7, ignore);
	int iFlags = 0;
	if ( n >= 8 ) iFlags = lua_tointeger(L, 8);

	int iHitObj = 0, iHitE = 0, iLimb = -1;
	float fHitX = fEndX, fHitY = fEndY, fHitZ = fEndZ, fNX = 0, fNY = 0, fNZ = 0;

	// the ground first, which shortens the ray
	if ( (iFlags & 1) && ODERayTerrain(fX, fY, fZ, fEndX, fEndY, fEndZ, false) == 1 )
	{
		fEndX = fHitX = ODEGetRayCollisionX(); fEndY = fHitY = ODEGetRayCollisionY(); fEndZ = fHitZ = ODEGetRayCollisionZ();
		fNX = ODEGetRayNormalX(); fNY = ODEGetRayNormalY(); fNZ = ODEGetRayNormalZ();
		iHitObj = -1;
	}

	// then the objects up to it, passing collisionmode 11 entities if asked (a few at most)
	extern std::vector<int> g_IntersectExtraIgnoreObjects;
	for ( int iTry = 0; iTry < 8; iTry++ )
	{
		g_IntersectExtraIgnoreObjects.clear();
		for (int i = 1; i < (int)ignore.size(); i++) g_IntersectExtraIgnoreObjects.push_back(ignore[i]);
		int iIgnoreObjNo = ignore.size() > 0 ? ignore[0] : 0;
		int iObj = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fX, fY, fZ, fEndX, fEndY, fEndZ, iIgnoreObjNo, 0, 0, 0, 0, true);
		g_IntersectExtraIgnoreObjects.clear();
		if ( iObj == 0 ) break;
		int iObjE = 0;
		if ( iObj > 0 )
		{
			for ( int e = 1; e <= g.entityelementlist; e++ )
			{
				if ( t.entityelement[e].obj == iObj ) { iObjE = e; break; }
			}
			if ( (iFlags & 2) && iObjE > 0 && t.entityprofile[t.entityelement[iObjE].bankindex].collisionmode == 11 )
			{
				ignore.push_back(iObj);
				continue;
			}
		}
		iHitObj = iObj;
		iHitE = iObjE;
		fHitX = ChecklistFValueA(6); fHitY = ChecklistFValueB(6); fHitZ = ChecklistFValueC(6);
		fNX = ChecklistFValueA(7); fNY = ChecklistFValueB(7); fNZ = ChecklistFValueC(7);
		iLimb = ( iObj > 0 ) ? ChecklistValueB(1) : -1;
		break;
	}

	lua_pushinteger(L, iHitObj);
	lua_pushinteger(L, iHitE);
	lua_pushnumber(L, fHitX);
	lua_pushnumber(L, fHitY);
	lua_pushnumber(L, fHitZ);
	lua_pushnumber(L, fNX);
	lua_pushnumber(L, fNY);
	lua_pushnumber(L, fNZ);
	lua_pushinteger(L, iLimb);
	return 9;
}

// RayRegionBegin(x, y, z, radius): lists the objects whose bounds reach the sphere, for RayRegion until the frame ends;
// returns how many
int RayRegionBegin(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	lua_pushinteger(L, WickedCall_BeginRegionRays(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4)));
	return 1;
}

// RayRegion(x1, y1, z1, x2, y2, z2 [, ignore]): a full-accuracy pick against RayRegionBegin's objects only, with
// IntersectRay's returns (no terrain). Hits nothing outside the frame of RayRegionBegin
int RayRegion(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 6) return 0;
	float fX = lua_tonumber(L, 1), fY = lua_tonumber(L, 2), fZ = lua_tonumber(L, 3);
	float fEndX = lua_tonumber(L, 4), fEndY = lua_tonumber(L, 5), fEndZ = lua_tonumber(L, 6);
	std::vector<int> ignore;
	if (n >= 7) LuaReadIntersectIgnore(L, 7, ignore);
	extern int g_iCurrentGunObj;
	if (g_iCurrentGunObj > 0) ignore.push_back(g_iCurrentGunObj);

	int iHitObj = 0, iHitE = 0, iLimb = -1;
	float fHitX = fEndX, fHitY = fEndY, fHitZ = fEndZ, fNX = 0, fNY = 0, fNZ = 0;
	float fDX = fEndX - fX, fDY = fEndY - fY, fDZ = fEndZ - fZ;
	float fLength = sqrtf(fDX * fDX + fDY * fDY + fDZ * fDZ);
	float fOutX, fOutY, fOutZ, fOutNX, fOutNY, fOutNZ;
	DWORD dwObj = 0;
	if (fLength > 0 && WickedCall_RegionRay(fX, fY, fZ, fDX, fDY, fDZ, fLength, ignore.data(), (int)ignore.size(), &fOutX, &fOutY, &fOutZ, &fOutNX, &fOutNY, &fOutNZ, &dwObj))
	{
		// as IntersectAllEx reports it: an entity's object, -1 for geometry with no object, nothing for other objects
		if (dwObj == 0) iHitObj = -1;
		else if ((int)dwObj >= g.entityviewstartobj && (int)dwObj <= g.entityviewendobj) iHitObj = (int)dwObj;
		if (iHitObj != 0)
		{
			fHitX = fOutX; fHitY = fOutY; fHitZ = fOutZ;
			fNX = fOutNX; fNY = fOutNY; fNZ = fOutNZ;
			extern int g_iWickedCallRayLimbHit;
			if (iHitObj > 0) iLimb = g_iWickedCallRayLimbHit;
		}
		if (iHitObj > 0)
		{
			for (int e = 1; e <= g.entityelementlist; e++)
			{
				if (t.entityelement[e].obj == iHitObj) { iHitE = e; break; }
			}
		}
	}

	lua_pushinteger(L, iHitObj);
	lua_pushinteger(L, iHitE);
	lua_pushnumber(L, fHitX);
	lua_pushnumber(L, fHitY);
	lua_pushnumber(L, fHitZ);
	lua_pushnumber(L, fNX);
	lua_pushnumber(L, fNY);
	lua_pushnumber(L, fNZ);
	lua_pushinteger(L, iLimb);
	return 9;
}

int RayRegionEnd(lua_State* L)
{
	WickedCall_EndRegionRays();
	return 0;
}

int IntersectGetLastHitBone(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1)
	{
		lua_pushstring(L, "");
		return 1;
	}
	int iObjectID = lua_tonumber(L,1);
	extern std::unordered_map<int, sFrame*> lastHitFrame;
	if (lastHitFrame.count(iObjectID) > 0)
	{

		if (ObjectExist(iObjectID))
		{
			sObject* pObject = g_ObjectList[iObjectID];
			sFrame* pFrame = lastHitFrame[iObjectID];
			//PE: Is frame still valid ?
			for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
			{
				if (pFrame == pObject->ppFrameList[iFrameIndex])
				{
					if (pFrame->pMesh && pFrame->pMesh->pBones)
					{
						if (strlen(pFrame->pMesh->pBones->szName) > 0)
						{
							//PE: Return name of first bone in list.
							lua_pushstring(L, pFrame->pMesh->pBones->szName);
							return 1;
						}
					}
					break;
				}
			}
		}
	}

	lua_pushstring(L, "");
	return 1;
}

int IntersectGetLastHitFrame(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1)
	{
		lua_pushstring(L, "");
		return 1;
	}
	int iObjectID = lua_tonumber(L, 1);
	extern std::unordered_map<int, sFrame*> lastHitFrame;
	if (lastHitFrame.count(iObjectID) > 0)
	{

		if (ObjectExist(iObjectID))
		{
			sObject* pObject = g_ObjectList[iObjectID];
			sFrame* pFrame = lastHitFrame[iObjectID];
			//PE: Is frame still valid ?
			for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
			{
				if (pFrame == pObject->ppFrameList[iFrameIndex])
				{
					if (pFrame->szName)
					{
						if (strlen(pFrame->szName) > 0)
						{
							//PE: Return bone name.
							lua_pushstring(L, pFrame->szName);
							return 1;
						}
					}
					break;
				}
			}
		}
	}

	lua_pushstring(L, "");
	return 1;
}

int IntersectAll ( lua_State *L )
{
	return IntersectCore ( L, 3 );
}
int IntersectStatic ( lua_State *L )
{
	return IntersectCore ( L, 1 );
}
int IntersectStaticPerformant (lua_State *L)
{
	return IntersectCore (L, 2);
}
int IntersectAllIncludeTerrain (lua_State* L)
{
	return IntersectCore (L, 0);
}
int GetIntersectCollisionX ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueA(6) );
	return 1;
}
int GetIntersectCollisionY ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueB(6) );
	return 1;
}
int GetIntersectCollisionZ ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueC(6) );
	return 1;
}
int GetIntersectCollisionNX ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueA(7) );
	return 1;
}
int GetIntersectCollisionNY ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueB(7) );
	return 1;
}
int GetIntersectCollisionNZ ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueC(7) );
	return 1;
}
// GetIntersectCollisionLimb(): the limb of the object the last intersect hit, numbered as GetLimbName numbers them; -1 when
// it hit terrain or the pick was not the full-accuracy one (IntersectAllEx fills it from WickedCall_SentRay4)
int GetIntersectCollisionLimb ( lua_State *L )
{
	lua_pushinteger ( L, ChecklistValueB(1) );
	return 1;
}
int PositionCamera ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	PositionCamera ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int PointCamera ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	PointCamera ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int MoveCamera ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	MoveCamera ( lua_tonumber(L, 1), lua_tonumber(L, 2) );
	return 0;
}
int GetObjectExist ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, ObjectExist(lua_tonumber(L, 1)) );
	return 1;
}

void gun_PlayObject(int iObjID, float fStart, float fEnd);
void gun_StopObject(int iObjID);
void gun_LoopObject(int iObjID, float fStart, float fEnd);
void gun_SetObjectFrame(int iObjID, float fValue);
void gun_SetObjectSpeed(int iObjID, float fValue);

float iGunAnimStart = 0;
float iGunAnimEnd = 0;
float fOldGunSpeed = 0;
int iGunAnimMode = 2; // 0 = Play , 1 = Loop, 2 = stop
extern bool bCustomGunAnimationRunning;
bool bForceGunUnderWater = false;

void GunInitAnimationSettings(void)
{
	iGunAnimStart = 0;
	iGunAnimEnd = 0;
	iGunAnimMode = 2;
	bCustomGunAnimationRunning = false;
	bForceGunUnderWater = false;
}
int ForceGunUnderWater(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	bForceGunUnderWater = lua_tonumber(L, 1);
	return 0;
}

int GetGunEmissiveStrength(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	lua_pushnumber(L, t.gun[t.gunid].settings.fEmissiveStrength);
	return 1;
}
int SetGunEmissiveStrength(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float emi = lua_tonumber(L, 1);
	sObject* pGunObject = GetObjectData(t.currentgunobj);
	if (pGunObject)
		WickedCall_SetObjectEmissiveStrength(pGunObject, emi);
	return 0;
}

int GetGunAnimationFramesFromName(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	char AnimName[512];
	float fFoundStart = 0, fFoundFinish = 0;

	const char* luastring = lua_tostring(L, 1);
	if(luastring)
		strcpy(AnimName, luastring);

	sObject* pObject = GetObjectData(t.currentgunobj);
	extern int entity_lua_getanimationnamefromobject(sObject * pObject, cstr FindThisName_s, float* fFoundStart, float* fFoundFinish);
	if (pObject && luastring && entity_lua_getanimationnamefromobject(pObject, AnimName, &fFoundStart, &fFoundFinish) > 0)
	{
		lua_pushnumber(L, fFoundStart);
		lua_pushnumber(L, fFoundFinish);
	}
	else
	{
		//PE: Try to get it from the current weapon.
		if (stricmp(AnimName, "reload") == NULL)
		{
			if (g.firemodes[t.gunid][0].action.startreload.s > 0 && g.firemodes[t.gunid][0].action.startreload.e >= g.firemodes[t.gunid][0].action.startreload.s)
			{
				lua_pushnumber(L, g.firemodes[t.gunid][0].action.startreload.s);
				lua_pushnumber(L, g.firemodes[t.gunid][0].action.startreload.e);
				return 2;
			}
		}
		if (stricmp(AnimName, "idle") == NULL)
		{
			if (g.firemodes[t.gunid][0].action.idle.s > 0 && g.firemodes[t.gunid][0].action.idle.e >= g.firemodes[t.gunid][0].action.idle.s)
			{
				lua_pushnumber(L, g.firemodes[t.gunid][0].action.idle.s);
				lua_pushnumber(L, g.firemodes[t.gunid][0].action.idle.e);
				return 2;
			}
		}
		if (stricmp(AnimName, "fire") == NULL)
		{
			if (g.firemodes[t.gunid][0].action.start.s > 0 && g.firemodes[t.gunid][0].action.start.e >= g.firemodes[t.gunid][0].action.start.s)
			{
				lua_pushnumber(L, g.firemodes[t.gunid][0].action.start.s);
				lua_pushnumber(L, g.firemodes[t.gunid][0].action.start.e);
				return 2;
			}
		}

		lua_pushnumber(L, 0);
		lua_pushnumber(L, 0);
	}
	return 2;
}
int GunAnimationSetFrame(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float start = lua_tonumber(L, 1);
	gun_SetObjectFrame(t.currentgunobj, start);
	return 0;
}
int GunAnimationPlaying(lua_State* L)
{
	int n = LUA_GETTOP(L);
	//if (n < 1) return 0;
	float frame = GetFrame(t.currentgunobj);
	if (iGunAnimMode == 0)
	{
		//PE: Stop animation when done.

		bool playing = false;
		if (t.currentgunobj > 0)
		{
			sObject* pObject = g_ObjectList[t.currentgunobj];
			playing = WickedCall_GetAnimationPlayingState(pObject);
		}

		if (frame >= iGunAnimEnd || !playing)
		{
			gun_StopObject(t.currentgunobj);
			gun_SetObjectFrame(t.currentgunobj, iGunAnimEnd);
			if (fOldGunSpeed > 1)
				gun_SetObjectSpeed(t.currentgunobj, fOldGunSpeed);
			fOldGunSpeed = 0;
			lua_pushnumber(L, 0);
			bCustomGunAnimationRunning = false;
			t.gunmode = 9; //PE: switch to idle.
			iGunAnimMode = 3;
		}
		else
		{
			lua_pushnumber(L, 1);
			bCustomGunAnimationRunning = true;
		}

	}
	else if(iGunAnimMode == 1)
	{
		//PE: Looping always playing.
		lua_pushnumber(L, 1);
		bCustomGunAnimationRunning = true;
	}
	else
	{
		//PE: Stopped.
		lua_pushnumber(L, 0);
		bCustomGunAnimationRunning = false;
	}
	return 1;

}
int SetGunAnimationSpeed(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float speed = lua_tonumber(L, 1);
	if(fOldGunSpeed == 0)
		fOldGunSpeed = GetSpeed(t.currentgunobj);
	gun_SetObjectSpeed(t.currentgunobj, speed);
	return 0;
}
int PlayGunAnimation(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float start = lua_tonumber(L, 1);
	float end = lua_tonumber(L, 2);
	if (start > end)
	{
		float tmp = start;
		start = end;
		end = tmp;
	}
	gun_PlayObject(t.currentgunobj, start, end);
	bCustomGunAnimationRunning = true;
	iGunAnimStart = start;
	iGunAnimEnd = end;
	iGunAnimMode = 0;
	return 0;
}
int StopGunAnimation(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	gun_StopObject(t.currentgunobj);
	if (fOldGunSpeed > 1)
		gun_SetObjectSpeed(t.currentgunobj, fOldGunSpeed);
	fOldGunSpeed = 0;
	iGunAnimMode = 2;
	bCustomGunAnimationRunning = false;
	t.gunmode = 9; //PE: switch to idle.
	return 0;
}
int LoopGunAnimation(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float start = lua_tonumber(L, 1);
	float end = lua_tonumber(L, 2);
	if (start > end)
	{
		float tmp = start;
		start = end;
		end = tmp;
	}
	gun_LoopObject(t.currentgunobj, start, end);
	bCustomGunAnimationRunning = true;
	iGunAnimStart = start;
	iGunAnimEnd = end;
	iGunAnimMode = 1;
	return 0;
}



int SetObjectFrame (lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	SetObjectFrame (lua_tonumber(L, 1), lua_tonumber(L, 2));
	return 0;
}
int GetObjectFrame ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, GetFrame(lua_tonumber(L, 1)) );
	return 1;
}
int SetObjectSpeed ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	SetObjectSpeed ( lua_tonumber(L, 1), lua_tonumber(L, 2) );
	return 0;
}
int GetObjectSpeed ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, GetSpeed(lua_tonumber(L, 1)) );
	return 1;
}
int PositionObject ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	PositionObject ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int ScaleObjectXYZ(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	ScaleObject(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	return 0;
}
// Add Fast Quaternion functions
int QuatMultiply(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 8) return 0;

	GGQUATERNION q1( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	GGQUATERNION q2( lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8) );
	
	float A = ( q1.w + q1.x ) * ( q2.w + q2.x );
	float B = ( q1.z - q1.y ) * ( q2.y - q2.z );
	float C = ( q1.w - q1.x ) * ( q2.y + q2.z );
	float D = ( q1.y + q1.z ) * ( q2.w - q2.x );
	float E = ( q1.x + q1.z ) * ( q2.x + q2.y );
	float F = ( q1.x - q1.z ) * ( q2.x - q2.y );
	float G = ( q1.w + q1.y ) * ( q2.w - q2.z );
	float H = ( q1.w - q1.y ) * ( q2.w + q2.z );

	q1.w = B + (-E - F + G + H ) / 2;
	q1.x = A - ( E + F + G + H ) / 2;
	q1.y = C + ( E - F + G - H ) / 2;
	q1.z = D + ( E - F - G + H ) / 2;

	lua_pushnumber( L, q1.x );
	lua_pushnumber( L, q1.y );
	lua_pushnumber( L, q1.z );
	lua_pushnumber( L, q1.w );
	return 4;
}
int QuatToEuler(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;

	GGQUATERNION q( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );

	float sqw = q.w * q.w;
	float sqx = q.x * q.x;
	float sqy = q.y * q.y;
	float sqz = q.z * q.z;

	float h = -2.0 * ( q.x * q.z - q.y * q.w );

	float x, y, z;

	x = -atan2( 2.0 * ( q.y * q.z + q.x * q.w ), ( -sqx - sqy + sqz + sqw ) );
	z = -atan2( 2.0 * ( q.x * q.y + q.z * q.w ), (  sqx - sqy - sqz + sqw ) );

	if ( abs( h ) < 0.99999 )
	{		
		y =  asin( -2.0 * ( q.x * q.z - q.y * q.w ) );
	}
	else
	{
		y = ( PI / 2 ) * h;
	}

	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	return 3;
}
int EulerToQuat(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float pitch = lua_tonumber( L, 1 );
	float yaw   = lua_tonumber( L, 2 );
	float roll  = lua_tonumber( L, 3 );

	float sr = sin( roll  / 2.0 );
	float sp = sin( pitch / 2.0 );
	float sy = sin( yaw   / 2.0 );
	float cr = cos( roll  / 2.0 );
	float cp = cos( pitch / 2.0 );
	float cy = cos( yaw   / 2.0 );

	float cycp = cy * cp;
	float sysp = sy * sp;
	float sycp = sy * cp;
	float cysp = cy * sp;

	lua_pushnumber( L, ( sr * sycp ) - ( cr * cysp ) );  // q.x
	lua_pushnumber( L, ( sr * cysp ) + ( cr * sycp ) );  // q.y
	lua_pushnumber( L, ( cr * sysp ) - ( sr * cycp ) );  // q.z
	lua_pushnumber( L, ( sr * sysp ) + ( cr * cycp ) );  // q.w
	return 4;
}
// QuatToEuler and EulerToQuat work in radians and turn pitch and roll the opposite way to the engine's angles (they
// agree with each other). AnglesToQuat and QuatToAngles use the engine's own: degrees, rotated X then Y then Z, as
// SetRotation and GetEntityAngleX/Y/Z and every object, with quaternions as QuatMultiply and QuatSLERP take them
int AnglesToQuat(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	GGMATRIX matRotateX, matRotateY, matRotateZ;
	GGMatrixRotationX( &matRotateX, GGToRadian( lua_tonumber( L, 1 ) ) );
	GGMatrixRotationY( &matRotateY, GGToRadian( lua_tonumber( L, 2 ) ) );
	GGMatrixRotationZ( &matRotateZ, GGToRadian( lua_tonumber( L, 3 ) ) );
	GGMATRIX m = matRotateX * matRotateY * matRotateZ;

	// the quaternion GGMatrixRotationQuaternion turns back into this matrix
	float x, y, z, w;
	float fTrace = m._11 + m._22 + m._33;
	if ( fTrace > 0.0f )
	{
		float s = sqrtf( fTrace + 1.0f ) * 2.0f;
		w = 0.25f * s; x = ( m._23 - m._32 ) / s; y = ( m._31 - m._13 ) / s; z = ( m._12 - m._21 ) / s;
	}
	else if ( m._11 > m._22 && m._11 > m._33 )
	{
		float s = sqrtf( 1.0f + m._11 - m._22 - m._33 ) * 2.0f;
		w = ( m._23 - m._32 ) / s; x = 0.25f * s; y = ( m._12 + m._21 ) / s; z = ( m._31 + m._13 ) / s;
	}
	else if ( m._22 > m._33 )
	{
		float s = sqrtf( 1.0f + m._22 - m._11 - m._33 ) * 2.0f;
		w = ( m._31 - m._13 ) / s; x = ( m._12 + m._21 ) / s; y = 0.25f * s; z = ( m._23 + m._32 ) / s;
	}
	else
	{
		float s = sqrtf( 1.0f + m._33 - m._11 - m._22 ) * 2.0f;
		w = ( m._12 - m._21 ) / s; x = ( m._31 + m._13 ) / s; y = ( m._23 + m._32 ) / s; z = 0.25f * s;
	}
	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	lua_pushnumber( L, w );
	return 4;
}
int QuatToAngles(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	GGQUATERNION q( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	float fLength = sqrtf( q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w );
	if ( fLength > 0.0f ) { q.x /= fLength; q.y /= fLength; q.z /= fLength; q.w /= fLength; }
	GGMATRIX matRotation;
	GGMatrixRotationQuaternion( &matRotation, &q );
	// the engine's matrix to angles, as an object's free rotation is turned back into its angles
	extern void AnglesFromMatrix( GGMATRIX* pmatMatrix, GGVECTOR3* pVecAngles );
	GGVECTOR3 vecAngles = GGVECTOR3( 0, 0, 0 );
	AnglesFromMatrix( &matRotation, &vecAngles );
	lua_pushnumber( L, vecAngles.x );
	lua_pushnumber( L, vecAngles.y );
	lua_pushnumber( L, vecAngles.z );
	return 3;
}
int QuatSLERP(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 9) return 0;
	const GGQUATERNION qa(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	const GGQUATERNION qb(lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8));

	GGQUATERNION qOut;

	QuaternionSlerp( &qOut, &qa, &qb, lua_tonumber(L, 9) );

	lua_pushnumber(L, qOut.x);
	lua_pushnumber(L, qOut.y);
	lua_pushnumber(L, qOut.z);
	lua_pushnumber(L, qOut.w);
	return 4;
}
int QuatLERP(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 9) return 0;

	const GGQUATERNION qa(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	const GGQUATERNION qb(lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8));
	float t = lua_tonumber(L, 9);

	float at = 1.0 - t;
	float bt = t;
	if (qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w < 0) 
	{
		bt = -t;
	}

	lua_pushnumber(L, qa.x * at + qb.x * bt );
	lua_pushnumber(L, qa.y * at + qb.y * bt );
	lua_pushnumber(L, qa.z * at + qb.z * bt );
	lua_pushnumber(L, qa.w * at + qb.w * bt );
	return 4;
}

int ScreenCoordsToPercent(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fPercentX = ( fX / (float) g_dwScreenWidth) * 100.0f;
	float fPercentY = (fY / (float)g_dwScreenHeight) * 100.0f;
	lua_pushnumber(L, fPercentX);
	lua_pushnumber(L, fPercentY);
	return 2;

}

int LuaConvert2DTo3D(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	
	float x = (fX * g_dwScreenWidth) / 100.0f;
	float y = (fY * g_dwScreenHeight) / 100.0f;

	//PE: Wicked GetPickRay use dpi so must add it here.
	const float dpiscaling = (float)GetDpiForWindow(g_pGlob->hWnd) / 96.0f;
	x /= dpiscaling;
	y /= dpiscaling;

	float fOutX = 0, fOutY = 0, fOutZ = 0;
	float fDirX = 0, fDirY = 0, fDirZ = 0;
	Convert2Dto3D(x, y, &fOutX, &fOutY, &fOutZ, &fDirX, &fDirY, &fDirZ);

	lua_pushnumber(L, fOutX);
	lua_pushnumber(L, fOutY);
	lua_pushnumber(L, fOutZ);
	lua_pushnumber(L, fDirX);
	lua_pushnumber(L, fDirY);
	lua_pushnumber(L, fDirZ);
	return 6;
}


int LuaConvert3DTo2D(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	ImVec2 Convert3DTo2D(float x, float y, float z);
	ImVec2 v2DPos = Convert3DTo2D(fX, fY, fZ);
	lua_pushnumber(L, v2DPos.x);
	lua_pushnumber(L, v2DPos.y);
	return 2;
}

// end of Fast Quaternion functions
int RotateObject ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	RotateObject ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int GetObjectAngleX ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, ObjectAngleX(lua_tonumber(L, 1)) );
	return 1;
}
int GetObjectAngleY ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, ObjectAngleY(lua_tonumber(L, 1)) );
	return 1;
}
int GetObjectAngleZ ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, ObjectAngleZ(lua_tonumber(L, 1)) );
	return 1;
}
int GetObjectPosAng( lua_State *L )
{
	int n = lua_gettop( L );
	if (n < 1) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance(iID) )
	{
		// seems can be called in LUA when object not exist, so return zeros
		lua_pushnumber ( L, 0 );
		lua_pushnumber ( L, 0 );
		lua_pushnumber ( L, 0 );
		lua_pushnumber ( L, 0 );
		lua_pushnumber ( L, 0 );
		lua_pushnumber ( L, 0 );
		return 6;
	}
	else
	{
		// object information
		sObject* pObject = g_ObjectList[iID];
		lua_pushnumber ( L, pObject->position.vecPosition.x );
		lua_pushnumber ( L, pObject->position.vecPosition.y );
		lua_pushnumber ( L, pObject->position.vecPosition.z );
		lua_pushnumber ( L, pObject->position.vecRotate.x );
		lua_pushnumber ( L, pObject->position.vecRotate.y );
		lua_pushnumber ( L, pObject->position.vecRotate.z );
	}
	return 6;
}
int GetObjectColBox( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber( L, 1 );
	if (!ConfirmObjectInstance( iID ) )
		return 0;
	// object information
	sObject* pObject = g_ObjectList[iID];

	lua_pushnumber( L, pObject->collision.vecMin.x );
	lua_pushnumber( L, pObject->collision.vecMin.y );
	lua_pushnumber( L, pObject->collision.vecMin.z );
	lua_pushnumber( L, pObject->collision.vecMax.x );
	lua_pushnumber( L, pObject->collision.vecMax.y );
	lua_pushnumber( L, pObject->collision.vecMax.z );
	return 6;
}
int GetObjectCentre( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	// object information
	sObject* pObject = g_ObjectList[iID];

	lua_pushnumber(L, pObject->collision.vecCentre.x);
	lua_pushnumber(L, pObject->collision.vecCentre.y);
	lua_pushnumber(L, pObject->collision.vecCentre.z);
	return 3;
}
int GetObjectColCentre(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID)) return 0;
	sObject* pObject = g_ObjectList[iID];
	UpdateColCenter(pObject);
	lua_pushnumber(L, pObject->collision.vecColCenter.x);
	lua_pushnumber(L, pObject->collision.vecColCenter.y);
	lua_pushnumber(L, pObject->collision.vecColCenter.z);
	return 3;
}
int GetObjectScales( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	// object information
	sObject* pObject = g_ObjectList[iID];

	lua_pushnumber(L, pObject->position.vecScale.x);
	lua_pushnumber(L, pObject->position.vecScale.y);
	lua_pushnumber(L, pObject->position.vecScale.z);
	return 3;
}
int PushObject( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	if (n == 7)
	{
		ODEAddBodyForce( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
						 	  lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7));
	}
	else if (n == 4)
	{
		ODEAddBodyForce( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), 0, 0, 0 );
	}
	return 0;
}
int ConstrainObjMotion( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0; 
	ODEConstrainBodyMotion( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int ConstrainObjRotation( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance(iID) )
		return 0;
	ODEConstrainBodyRotation( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int CreateSingleHinge( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 7 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance(iID) )
		return 0;
	int iC = ODECreateHingeSingle( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
		                                lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7) );
	lua_pushnumber( L, iC );
	return 1;
}
int CreateDoubleHinge( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 11 ) return 0;
	int iIDa = lua_tonumber(L, 1);
	int iIDb = lua_tonumber(L, 2);
	if (!ConfirmObjectInstance(iIDa) || !ConfirmObjectInstance(iIDb))
		return 0;

	int iC = ODECreateHingeDouble( iIDa, iIDb, lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5),
		                                       lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8),
		                                       lua_tonumber(L, 9), lua_tonumber(L, 10), lua_tonumber(L, 11) );
	lua_pushnumber( L, iC );
	return 1;
}
int CreateSingleJoint( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 4 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;

	int iC = ODECreateJointSingle( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	lua_pushnumber( L, iC );
	return 1;
}
int CreateDoubleJoint( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 9 ) return 0;
	int iIDa = lua_tonumber( L, 1 );
	int iIDb = lua_tonumber( L, 2 );
	if ( !ConfirmObjectInstance( iIDa ) || !ConfirmObjectInstance( iIDb ) )
		return 0;

	int iC = ODECreateJointDouble( iIDa, iIDb, lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5),
		                                       lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8),
		                                       lua_tonumber(L, 9) );
	lua_pushnumber( L, iC );
	return 1;
}
int CreateSliderDouble( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 17 ) return 0;
	int iIDa = lua_tonumber( L, 1 );
	int iIDb = lua_tonumber( L, 2 );
	if ( !ConfirmObjectInstance( iIDa ) || !ConfirmObjectInstance( iIDb ) )
		return 0;

	int iC = ODECreateSliderDouble( iIDa, iIDb, lua_tonumber(L,  3), lua_tonumber(L,  4), lua_tonumber(L,  5),
		                                        lua_tonumber(L,  6), lua_tonumber(L,  7), lua_tonumber(L,  8),
		                                        lua_tonumber(L,  9), lua_tonumber(L, 10), lua_tonumber(L, 11),
		                                        lua_tonumber(L, 12), lua_tonumber(L, 13), lua_tonumber(L, 14),
		                                        lua_tonumber(L, 15), lua_tonumber(L, 16), lua_tonumber(L, 17) == 1 );
	lua_pushnumber( L, iC );
	return 1;
}
int SetSliderLimits(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;
	int iC = lua_tonumber(L, 1);

	ODESetSliderLimits( iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5) );
	return 0;
}

int RemoveObjectConstraints( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;

	ODERemoveBodyConstraints( iID );
	return 0;
}
int RemoveConstraint( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iC = lua_tonumber( L, 1 );

	ODERemoveConstraint( iC );
	return 0;
}
int SetObjectDamping( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;

	ODESetBodyDamping( iID, lua_tonumber( L, 2 ), lua_tonumber( L, 3 ) );

	return 0;
}
int SetHingeLimits(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	int iC = lua_tonumber(L, 1);

	if (n < 4)
	{
		// Set angle min/max
		ODESetHingeLimits(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), 0.9f, 0.3f, 1.0f);
	}
	else if (n < 5)
	{
		// also set softness
		ODESetHingeLimits(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), 0.3f, 1.0f);
	}
	else if (n < 6)
	{
		// also set bias
		ODESetHingeLimits(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
			lua_tonumber(L, 5), 1.0f);
	}
	else
	{
		// also set relaxation
		ODESetHingeLimits(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
			lua_tonumber(L, 5), lua_tonumber(L, 6));
	}
	return 0;
}
int SetHingeMotor( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iC = lua_tonumber(L, 1);

	ODESetHingeMotor(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	return 0;
}
int SetSliderMotor(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iC = lua_tonumber(L, 1);

	ODESetSliderMotor( iC, lua_tonumber(L, 2) == 1, lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int GetHingeAngle(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iC = lua_tonumber(L, 1);

	lua_pushnumber(L, ODEGetHingeAngle(iC));
	return 1;
}
int GetSliderPosition(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iC = lua_tonumber(L, 1);

	lua_pushnumber( L, ODEGetSliderPosition( iC ) );
	return 1;
}
int SetBodyScaling(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iC = lua_tonumber(L, 1);

	ODESetBodyScaling( iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}

int PhysicsRayCast( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 7 ) return 0;
	float fFromX = lua_tonumber( L, 1 );
	float fFromY = lua_tonumber( L, 2 );
	float fFromZ = lua_tonumber( L, 3 );
	float fToX   = lua_tonumber( L, 4 );
	float fToY   = lua_tonumber( L, 5 );
	float fToZ   = lua_tonumber( L, 6 );
	float fForce = lua_tonumber( L, 7 );
	if ( ODERayForce( fFromX, fFromY, fFromZ, fToX, fToY, fToZ, fForce ) == 1 )
	{
		int iObjHit = ODEGetRayObjectHit();
		// only return dynamic objects
		if ( ODEGetBodyIsDynamic( iObjHit ) )
		{
			lua_pushnumber( L, iObjHit );
			lua_pushnumber( L, ODEGetRayCollisionX() );
			lua_pushnumber( L, ODEGetRayCollisionY() );
			lua_pushnumber( L, ODEGetRayCollisionZ() );
			return 4;
		}
	}
	lua_pushnumber( L, 0 );
	return 1;
}
// the object numbers a physics query ignores: one number, or a table of up to PHYSICS_QUERY_MAX_IGNORE (as IntersectRay's)
static int LuaPhysicsIgnoreList(lua_State* L, int iIndex, int* pIgnore)
{
	int iCount = 0;
	if (LUA_GETTOP(L) < iIndex) return 0;
	if (lua_istable(L, iIndex))
	{
		int iLength = (int)lua_rawlen(L, iIndex);
		for (int i = 1; i <= iLength && iCount < PHYSICS_QUERY_MAX_IGNORE; i++)
		{
			lua_rawgeti(L, iIndex, i);
			int iObj = (int)lua_tointeger(L, -1);
			lua_pop(L, 1);
			if (iObj > 0) pIgnore[iCount++] = iObj;
		}
	}
	else
	{
		int iObj = (int)lua_tointeger(L, iIndex);
		if (iObj > 0) pIgnore[iCount++] = iObj;
	}
	return iCount;
}

// the collision cylinders the physics keeps at the trees round the camera (physics_managevirtualtreecylinders): a hit on
// one is a tree trunk, reported as the map-wide trunks are
static bool LuaPhysicsIsTreeCylinder(int iObject)
{
	return iObject >= g.virtualtreeobjectstart && iObject < g.virtualtreeobjectstart + PHYSICS_VIRTUALTREE_MAX;
}

// hit, x, y, z, nx, ny, nz, obj, fraction, tree, treetype (iTreeType for a map-wide trunk, else the cylinder's)
static int LuaPushPhysicsHit(lua_State* L, const PhysicsHit& hit, bool bTree, int iTreeType)
{
	bool bTreeHit = hit.hit && (bTree || LuaPhysicsIsTreeCylinder(hit.object));
	extern int physics_virtualtreetype(int iObject);
	if (bTreeHit && !bTree) iTreeType = physics_virtualtreetype(hit.object);
	lua_pushinteger(L, hit.hit ? 1 : 0);
	for (int i = 0; i < 3; i++) lua_pushnumber(L, hit.hit ? hit.point[i] : 0.0f);
	for (int i = 0; i < 3; i++) lua_pushnumber(L, hit.hit ? hit.normal[i] : 0.0f);
	lua_pushinteger(L, (hit.hit && !bTreeHit) ? hit.object : 0);
	lua_pushnumber(L, hit.hit ? hit.fraction : 1.0f);
	lua_pushinteger(L, bTreeHit ? 1 : 0);
	lua_pushinteger(L, bTreeHit ? iTreeType : -1);
	return 11;
}

// PhysicsRay(x1, y1, z1, x2, y2, z2 [, layers [, ignore]]): the nearest body the segment meets in the physics world, with no
// force applied (PhysicsRayCast pushes what it hits and returns moving bodies only). layers add up: 1 the ground (and
// entities built as terrain colliders), 2 static entities, 4 characters, 8 moving bodies (11 unless set); ignore is an
// object number or a table of up to 8. Returns hit, x, y, z, nx, ny, nz, obj (0 the ground or a tree, -1 a character
// capsule), fraction, tree (1 for a trunk) and treetype (the trunk's tree type, GetTreeTypeName, -1 if none). The physics world has the ground only as detailed as the terrain round the
// camera, no body for collisionmode 11 or 12, and trunks only round the camera (RayTrees has them everywhere)
int PhysicsRay(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 6) return 0;
	float fFrom[3] = { (float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3) };
	float fTo[3] = { (float)lua_tonumber(L, 4), (float)lua_tonumber(L, 5), (float)lua_tonumber(L, 6) };
	int iLayers = (n >= 7) ? (int)lua_tointeger(L, 7) : PHYSICS_LAYER_DEFAULT;
	int pIgnore[PHYSICS_QUERY_MAX_IGNORE];
	int iIgnoreCount = LuaPhysicsIgnoreList(L, 8, pIgnore);
	PhysicsHit hit;
	PhysicsQuery_Ray(fFrom, fTo, iLayers, pIgnore, iIgnoreCount, &hit);
	return LuaPushPhysicsHit(L, hit, false, -1);
}

// PhysicsSweepBox(cx, cy, cz, hx, hy, hz, yaw, mx, my, mz [, layers [, ignore [, trees]]]): the first thing a box meets as it
// moves by mx, my, mz: the box centred on cx, cy, cz, reaching hx, hy, hz each way along its own axes and turned by yaw
// degrees about Y as an object is (GetObjectColBox gives a hull's). It meets the physics world as PhysicsRay does, and
// with trees 1 (the default) every drawn tree trunk on the map too, base to top; with trees 2 only each trunk's bare part,
// from its base up to its crown (GetTreesNear's crown), so a pass through a canopy is no hit. No gap between samples and no tunnelling, and the
// normal is the real contact's. Returns what PhysicsRay does, fraction being how far along the motion; 0 means the box
// already touched something, so lift a car's box clear of the ground or leave the ground out of layers
int PhysicsSweepBox(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 10) return 0;
	float fCentre[3] = { (float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3) };
	float fHalf[3] = { (float)lua_tonumber(L, 4), (float)lua_tonumber(L, 5), (float)lua_tonumber(L, 6) };
	float fYaw = (float)lua_tonumber(L, 7);
	float fMotion[3] = { (float)lua_tonumber(L, 8), (float)lua_tonumber(L, 9), (float)lua_tonumber(L, 10) };
	int iLayers = (n >= 11) ? (int)lua_tointeger(L, 11) : PHYSICS_LAYER_DEFAULT;
	int pIgnore[PHYSICS_QUERY_MAX_IGNORE];
	int iIgnoreCount = LuaPhysicsIgnoreList(L, 12, pIgnore);
	int iTrees = (n < 13) ? 1 : (int)lua_tointeger(L, 13);
	bool bBareTrunk = (iTrees == 2);
	PhysicsHit hit;
	PhysicsQuery_SweepBox(fCentre, fHalf, fYaw, fMotion, iLayers, pIgnore, iIgnoreCount, &hit);

	// the collision cylinders at the trees round the camera stand the tree's full height, so bare trunks sweep on past them
	// and take those trees from the map-wide trunks below
	while (bBareTrunk && hit.hit && LuaPhysicsIsTreeCylinder(hit.object) && iIgnoreCount < PHYSICS_QUERY_MAX_IGNORE)
	{
		pIgnore[iIgnoreCount++] = hit.object;
		PhysicsQuery_SweepBox(fCentre, fHalf, fYaw, fMotion, iLayers, pIgnore, iIgnoreCount, &hit);
	}
	if (bBareTrunk && hit.hit && LuaPhysicsIsTreeCylinder(hit.object)) hit.hit = false;

	// the map-wide trunks, where nearer
	bool bTree = false;
	float fTrunk[7];
	int iTreeType = -1;
	if (iTrees != 0 && GGTrees::GGTrees_SweepBoxTrunks(fCentre, fHalf, fYaw, fMotion, fTrunk, &iTreeType, bBareTrunk) && (!hit.hit || fTrunk[0] < hit.fraction))
	{
		hit.hit = true;
		hit.fraction = fTrunk[0];
		for (int i = 0; i < 3; i++) { hit.point[i] = fTrunk[1 + i]; hit.normal[i] = fTrunk[4 + i]; }
		hit.object = 0;
		bTree = true;
	}
	return LuaPushPhysicsHit(L, hit, bTree, iTreeType);
}

// PhysicsOverlapBox(cx, cy, cz, hx, hy, hz, yaw [, layers [, ignore [, trees]]]): what a box (as PhysicsSweepBox's) touches
// now, such as a car wedged against something. Returns count, trees, then the object numbers of the count bodies it touches
// (up to 32; 0 the ground, -1 a character capsule), trees being how many tree trunks it touches (with trees 1, the default;
// with trees 2 only their bare parts below the crowns)
int PhysicsOverlapBox(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 7) return 0;
	float fCentre[3] = { (float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3) };
	float fHalf[3] = { (float)lua_tonumber(L, 4), (float)lua_tonumber(L, 5), (float)lua_tonumber(L, 6) };
	float fYaw = (float)lua_tonumber(L, 7);
	int iLayers = (n >= 8) ? (int)lua_tointeger(L, 8) : PHYSICS_LAYER_DEFAULT;
	int pIgnore[PHYSICS_QUERY_MAX_IGNORE];
	int iIgnoreCount = LuaPhysicsIgnoreList(L, 9, pIgnore);
	int iTreesMode = (n < 10) ? 1 : (int)lua_tointeger(L, 10);
	int pObjects[32];
	int iFound = PhysicsQuery_OverlapBox(fCentre, fHalf, fYaw, iLayers, pIgnore, iIgnoreCount, pObjects, 32);

	// the collision cylinders at the trees round the camera are trunks, counted with the map-wide ones
	int iCount = 0;
	for (int i = 0; i < iFound; i++)
	{
		if (!LuaPhysicsIsTreeCylinder(pObjects[i])) pObjects[iCount++] = pObjects[i];
	}
	int iTrees = (iTreesMode != 0) ? GGTrees::GGTrees_OverlapBoxTrunks(fCentre, fHalf, fYaw, iTreesMode == 2) : 0;
	lua_pushinteger(L, iCount);
	lua_pushinteger(L, iTrees);
	for (int i = 0; i < iCount; i++) lua_pushinteger(L, pObjects[i]);
	return 2 + iCount;
}

// GetPhysicsStats(): what the physics did in its last update, to find what a costly physics frame is working on. Returns
// subSteps (simulation steps taken, at most 7), awake (bodies moving or settling), manifolds (body pairs in contact),
// points (their contact points), then busiestObject and busiestPoints: the object with the most contact points, the
// ground and character capsules left out (-1 none), and its points
int GetPhysicsStats(lua_State* L)
{
	PhysicsStats stats;
	PhysicsQuery_Stats(&stats);
	lua_pushinteger(L, stats.subSteps);
	lua_pushinteger(L, stats.awakeBodies);
	lua_pushinteger(L, stats.manifolds);
	lua_pushinteger(L, stats.contactPoints);
	lua_pushinteger(L, stats.busiestObject);
	lua_pushinteger(L, stats.busiestPoints);
	return 6;
}
// GetPhysicsBodyPose(obj): where an object's physics body is, as the pose it would give the object: x, y, z and angles
// x, y, z in degrees (as GetEntityAngleX/Y/Z), then bodies, how many bodies the physics holds for the object (more than
// 1 is one left behind; the pose is the first's, the one CollisionOff removes). A static body never moves its object, so
// this shows the collider against the drawn object. With no body: nil for the pose, and 0
int GetPhysicsBodyPose(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	float fPos[3], fAng[3];
	int iBodies = ODEGetBodyPose((int)lua_tointeger(L, 1), fPos, fAng);
	for (int i = 0; i < 3; i++) { if (iBodies > 0) lua_pushnumber(L, fPos[i]); else lua_pushnil(L); }
	for (int i = 0; i < 3; i++) { if (iBodies > 0) lua_pushnumber(L, fAng[i]); else lua_pushnil(L); }
	lua_pushinteger(L, iBodies);
	return 7;
}
int GetObjectNumCollisions(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	lua_pushnumber(L, ODEGetBodyNumCollisions(iID));
	return 1;
}
int GetObjectCollisionDetails( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;
	int colNum = 0;
	int iColObj = 0;
	float fX, fY, fZ, fF;
	if (n == 2) colNum = lua_tonumber(L, 2);
	ODEGetBodyCollisionDetails( iID, colNum, iColObj, fX, fY, fZ, fF );
	lua_pushnumber( L, iColObj );
	lua_pushnumber( L, fX );
	lua_pushnumber( L, fY );
	lua_pushnumber( L, fZ );
	lua_pushnumber( L, fF );

	return 5;
}
int GetTerrainNumCollisions( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;
	lua_pushnumber( L, ODEGetTerrainNumCollisions( iID ) );
	return 1;
}
int GetTerrainCollisionDetails( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;
	int colNum = 0;
	int iLatest = 0;
	float fX, fY, fZ;
	if (n == 2) colNum = lua_tonumber( L, 2 );
	ODEGetTerrainCollisionDetails( iID, colNum, iLatest, fX, fY, fZ );
	lua_pushnumber( L, iLatest );
	lua_pushnumber( L, fX );
	lua_pushnumber( L, fY );
	lua_pushnumber( L, fZ );

	return 4;
}
int AddObjectCollisionCheck( lua_State *L )
{
	int n = lua_gettop( L );
	if (n < 1) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;
	ODEAddBodyCollisionCheck( iID );
	return 0;
}
int RemoveObjectCollisionCheck(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	ODERemoveBodyCollisionCheck(iID);
	return 0;
}

// Lua control of dynamic light
// get the light number using entity e number 
// then use that in the other light functions
int GetEntityLightNumber( lua_State *L )
{
	lua = L;
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;

	// get lightentity e number
	int iID = lua_tonumber( L, 1 );
	if ( iID <= 0 ) return 0;

	for ( int i = 1; i <= g.infinilightmax; i++)
	{
		if ( t.infinilight[ i ].used == 1 && t.infinilight[ i ].e == iID )
		{
			lua_pushinteger( L, i );
			return 1;
		}
	}
	return 0;
}
int GetLightPosition( lua_State *L )
{
	lua = L;
	int n = lua_gettop( L );
	if ( n < 1 )
		return 0;

	// get light number
	int i = lua_tointeger( L, 1 );

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[ i ].used == 1 )
	{
		lua_pushnumber( L, t.infinilight[i].x );
		lua_pushnumber( L, t.infinilight[i].y );
		lua_pushnumber( L, t.infinilight[i].z );
		return 3;
	}
	return 0;
}

//PE: Needed function to make light rotate, now that we have strange Z int eular angles.
//RotateGlobalAngleY(ax,ay,az,NewAngleY)
int RotateGlobalAngleY(lua_State *L)
{
	lua = L;
	// get number of arguments
	int n = LUA_GETTOP(L);
	// Not enough params, return out
	if (n < 4)
		return 0;

	float fAngleX = lua_tonumber(L, 1);
	float fAngleY = lua_tonumber(L, 2);
	float fAngleZ = lua_tonumber(L, 3);
	
	//Matrix
	float fNewAngleY = lua_tonumber(L, 4);

	//PE: Rotate Matrix.

	//New eular.
	lua_pushnumber(L, fAngleX);
	lua_pushnumber(L, fAngleY);
	lua_pushnumber(L, fAngleZ);
	return 3;
}

int GetLightAngle(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1)
		return 0;

	// get light number
	int i = lua_tointeger(L, 1);

	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		float fAngleX = t.infinilight[i].f_angle_x;
		float fAngleY = t.infinilight[i].f_angle_y;
		float fAngleZ = t.infinilight[i].f_angle_z;

		void FixEulerZInverted(float &ax, float &ay, float &az);
		FixEulerZInverted(fAngleX, fAngleY, fAngleZ);

		#ifdef WICKEDENGINE
		fAngleX = (fAngleX / 180.0f) - 1.0f;
		fAngleY = (fAngleY / 180.0f) - 1.0f;
		fAngleZ = (fAngleZ / 180.0f) - 1.0f;
		#endif

		lua_pushnumber(L, fAngleX);
		lua_pushnumber(L, fAngleY);
		lua_pushnumber(L, fAngleZ);
		return 3;
	}
	return 0;
}
int GetLightEuler(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int i = lua_tointeger(L, 1);
	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		float fAngleX = t.infinilight[i].f_angle_x;
		float fAngleY = t.infinilight[i].f_angle_y;
		float fAngleZ = t.infinilight[i].f_angle_z;
		void FixEulerZInverted(float& ax, float& ay, float& az);
		FixEulerZInverted(fAngleX, fAngleY, fAngleZ);
		lua_pushnumber(L, fAngleX);
		lua_pushnumber(L, fAngleY);
		lua_pushnumber(L, fAngleZ);
		return 3;
	}
	return 0;
}
int GetLightRGB( lua_State *L )
{
	lua = L;
	int n = lua_gettop( L );
	if ( n < 1 )
		return 0;

	// get light number
	int i = lua_tointeger( L, 1 );

	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		lua_pushnumber( L, t.infinilight[i].colrgb.r );
		lua_pushnumber( L, t.infinilight[i].colrgb.g );
		lua_pushnumber( L, t.infinilight[i].colrgb.b );
		return 3;
	}
	return 0;
}
int GetLightRange(lua_State *L)
{
	lua = L;
	int n = lua_gettop( L );
	if ( n < 1 )
		return 0;

	// get light number
	int i = lua_tointeger( L, 1 );

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[ i ].used == 1 )
	{
		lua_pushnumber( L, t.infinilight[ i ].range );
		return 1;
	}
	return 0;
}
// uses light number from above
int SetLightPosition( lua_State *L )
{
	lua = L;
	// get number of arguments
	int n = lua_gettop( L );
	// Not enough params, return out
	if ( n < 4 )
		return 0;

	// get light number
	int i = lua_tonumber( L, 1 );

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[ i ].used == 1 )
	{
		t.infinilight[ i ].x = lua_tonumber( L, 2 );
		t.infinilight[ i ].y = lua_tonumber( L, 3 );
		t.infinilight[ i ].z = lua_tonumber( L, 4 );
	}
	return 0;
}

float QuickEulerWrapAngle(float Angle)
{
	float NewAngle = fmod(Angle, 360.0f);
	if (NewAngle < 0.0f)
		NewAngle += 360.0f;
	return NewAngle;
}

int SetLightAngle(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if (n < 4)
		return 0;

	// get light number
	int i = lua_tonumber(L, 1);

	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		t.infinilight[i].f_angle_x = lua_tonumber(L, 2);
		t.infinilight[i].f_angle_y = lua_tonumber(L, 3);
		t.infinilight[i].f_angle_z = lua_tonumber(L, 4);

		#ifdef WICKEDENGINE
		//PE: Range -1 to 1.0 , convert to 0-360.
		//PE: This makes it rotate like in Classic.
		t.infinilight[i].f_angle_x = (t.infinilight[i].f_angle_x + 1.0) * 180.0;
		t.infinilight[i].f_angle_y = (t.infinilight[i].f_angle_y + 1.0) * 180.0;
		t.infinilight[i].f_angle_z = (t.infinilight[i].f_angle_z + 1.0) * 180.0;
		#endif
	}
	return 0;
}
int SetLightEuler(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int i = lua_tonumber(L, 1);
	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		t.infinilight[i].f_angle_x = lua_tonumber(L, 2);
		t.infinilight[i].f_angle_y = lua_tonumber(L, 3);
		t.infinilight[i].f_angle_z = lua_tonumber(L, 4);
	}
	return 0;
}
int SetLightRGB( lua_State *L ) 
{
	lua = L;
	// get number of arguments
	int n = lua_gettop( L );
	// Not enough params, return out
	if ( n < 4 )
		return 0;

	// get light number
	int i = lua_tonumber( L, 1 );

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[ i ].used == 1 )
	{
		t.infinilight[ i ].colrgb.r = lua_tonumber( L, 2 );
		t.infinilight[ i ].colrgb.g = lua_tonumber( L, 3 );
		t.infinilight[ i ].colrgb.b = lua_tonumber( L, 4 );
	}
	return 0;
}

// SetLightShadow(light, on): whether the light casts shadows (1 or 0), for the same light numbers as SetLightRGB. A
// shadow is the costliest part of a point or spot light, so turn it off on lights that don't need one
int SetLightShadow(lua_State *L)
{
	if (LUA_GETTOP(L) < 2) return 0;
	int i = lua_tointeger(L, 1);
	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		t.infinilight[i].bCanShadow = lua_tointeger(L, 2) != 0;
	}
	return 0;
}

int SetLightRange( lua_State *L )
{
	lua = L;
	// get number of arguments
	int n = LUA_GETTOP(L);
	// Not enough params, return out
	if (n < 2)
		return 0;

	// get light number
	int i = lua_tointeger(L, 1);

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1 )
	{
		float rng = lua_tonumber(L, 2);
		if ( rng < 1.0f )
		{
			rng = 1.0f;
		}
		else if ( rng > 10000.0f )
		{
			rng = 10000.0f;
		}
		t.infinilight[ i ].range = rng;
	}
	return 0;
}

int RunCharLoop ( lua_State *L )
{
	// run character animation system
	char_loop ( );
	return 0;
}
int TriggerWaterRipple ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	g.decalx=lua_tonumber(L, 1);
	g.decaly=lua_tonumber(L, 2);
	g.decalz=lua_tonumber(L, 3);
	#ifdef WICKEDENGINE
	extern int g_iBlendMode;
	int storage = g_iBlendMode;
	g_iBlendMode = 5; // Additive.
	#endif
	decal_triggerwaterripple ( );
	#ifdef WICKEDENGINE
	g_iBlendMode = storage;
	#endif
	return 0;
}


// TriggerWaterRippleSize(x, y, z, sizex, sizey [, nx, ny, nz]): a ripple ring; with a surface normal it lies on that
// surface (a slope) instead of flat. Nothing checks for water, so it can also mark wet ground
int TriggerWaterRippleSize(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	g.decalx = lua_tonumber(L, 1);
	g.decaly = lua_tonumber(L, 2);
	g.decalz = lua_tonumber(L, 3);
	t.decalscalemodx = lua_tonumber(L, 4);
	t.decalscalemody = lua_tonumber(L, 5);
	extern bool g_bDecalOnSurfaceNormal;
	extern GGVECTOR3 g_vecDecalSurfaceNormal;
	if (n >= 8)
	{
		GGVECTOR3 vecNormal = GGVECTOR3(lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8));
		if (GGVec3Length(&vecNormal) > 0.001f)
		{
			GGVec3Normalize(&g_vecDecalSurfaceNormal, &vecNormal);
			g_bDecalOnSurfaceNormal = true;
		}
	}
	#ifdef WICKEDENGINE
	extern int g_iBlendMode;
	int storage = g_iBlendMode;
	g_iBlendMode = 5; // Additive.
	#endif
	decal_triggerwaterripplesize();
	#ifdef WICKEDENGINE
	g_iBlendMode = storage;
	#endif
	g_bDecalOnSurfaceNormal = false;
	return 0;
}
// SetDecalRange(units): decals (water splashes and ripples, impacts, blood) further than this from the camera are not
// made; 800 (about 20 m) unless set, back to that at the next level. GetDecalRange() reads it
int SetDecalRange(lua_State *L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	decal_setrange(lua_tointeger(L, 1));
	return 0;
}
int GetDecalRange(lua_State *L)
{
	lua_pushinteger(L, g.decalrange);
	return 1;
}
// SetDecalLimit(total [, ripples]): how many decals can show at once (100 unless raised, up to 349; they are made now and
// kept), and how many of those only water ripples may use (0, the default, lets ripples share them all; back to 0 at the
// next level). GetDecalLimit() returns total, ripples
int SetDecalLimit(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iRipples = 0;
	if (n >= 2) iRipples = lua_tointeger(L, 2);
	decal_setlimit(lua_tointeger(L, 1), iRipples);
	return 0;
}
int GetDecalLimit(lua_State *L)
{
	extern int g_iDecalRippleStart;
	lua_pushinteger(L, g.decalelementmax);
	lua_pushinteger(L, g_iDecalRippleStart > 0 ? g.decalelementmax - g_iDecalRippleStart + 1 : 0);
	return 2;
}
// GetDecalStats(): decals showing now, and since the level started those not made for being out of range and for finding
// no free element
int GetDecalStats(lua_State *L)
{
	extern int g_iDecalDroppedRange, g_iDecalDroppedFull;
	int iActive = 0;
	for (int i = 1; i <= g.decalelementmax && i < (int)t.decalelement.size(); i++)
	{
		if (t.decalelement[i].active == 1) iActive++;
	}
	lua_pushinteger(L, iActive);
	lua_pushinteger(L, g_iDecalDroppedRange);
	lua_pushinteger(L, g_iDecalDroppedFull);
	return 3;
}
// AddProjectedDecal(image, x, y, z, size [, nx, ny, nz [, spin [, depth [, life]]]]): a texture projected onto the terrain and
// objects around x, y, z, following slopes, steps and corners, such as a scorch mark. size is its width across the surface;
// nx, ny, nz the surface normal it projects along (0, 1, 0 by default, onto the ground); spin its turn about that normal in
// degrees; depth how far along the normal it reaches in all (half its size by default; it fades out towards both ends);
// life the seconds before it fades out and goes (0 or less, the default, for never). image is a texture path as models
// use them: a .dds with its mipmaps, or a .png. Returns its id, 0 if the texture did not load. The oldest go first past
// SetProjectedDecalLimit, and none stay past the level
int AddProjectedDecal(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;
	const char* pImage = lua_tostring(L, 1);
	if (!pImage) return 0;
	float fNX = 0, fNY = 1, fNZ = 0, fSpin = 0, fDepth = 0, fLife = -1;
	if (n >= 8) { fNX = lua_tonumber(L, 6); fNY = lua_tonumber(L, 7); fNZ = lua_tonumber(L, 8); }
	if (n >= 9) fSpin = lua_tonumber(L, 9);
	if (n >= 10) fDepth = lua_tonumber(L, 10);
	if (n >= 11) { fLife = lua_tonumber(L, 11); if (fLife <= 0) fLife = -1; }
	char pImagePath[MAX_PATH];
	strcpy_s(pImagePath, MAX_PATH, pImage);
	int iID = WickedCall_AddProjectedDecal(pImagePath, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), fNX, fNY, fNZ, fSpin, fDepth, fLife);
	lua_pushinteger(L, iID);
	return 1;
}
// the length of the line between two points
static float BlastDecalDistance(const float* pA, const float* pB)
{
	float fDX = pB[0] - pA[0], fDY = pB[1] - pA[1], fDZ = pB[2] - pA[2];
	return sqrtf(fDX * fDX + fDY * fDY + fDZ * fDZ);
}

// a short ray for a blast decal: how far along it the first surface lies (the ground, or one of the objects that take
// decals WickedCall_FindDecalRayTargets found), as a fraction of its length (1 for none), with that surface's normal in
// pNormal. Tree trunks don't count, as they take no decals
static float BlastDecalRay(const float* pFrom, const float* pDir, float fLength, float* pNormal)
{
	float fTo[3] = { pFrom[0] + pDir[0] * fLength, pFrom[1] + pDir[1] * fLength, pFrom[2] + pDir[2] * fLength };
	float fNearest = 1.0f;
	// the ground only (1, COL_TERRAIN): ODERayTerrain would report a trunk in front of it instead
	if (ODERayTerrainEx(pFrom[0], pFrom[1], pFrom[2], fTo[0], fTo[1], fTo[2], 1, false) == 1)
	{
		float fHit[3] = { ODEGetRayCollisionX(), ODEGetRayCollisionY(), ODEGetRayCollisionZ() };
		fNearest = BlastDecalDistance(pFrom, fHit) / fLength;
		pNormal[0] = ODEGetRayNormalX(); pNormal[1] = ODEGetRayNormalY(); pNormal[2] = ODEGetRayNormalZ();
	}
	float fDistance = 0.0f;
	float fObjectNormal[3] = { 0, 0, 0 };
	if (WickedCall_DecalRayTargetsRay(pFrom, pDir, fLength, &fDistance, fObjectNormal))
	{
		float fAlong = fDistance / fLength;
		if (fAlong < fNearest)
		{
			fNearest = fAlong;
			pNormal[0] = fObjectNormal[0]; pNormal[1] = fObjectNormal[1]; pNormal[2] = fObjectNormal[2];
		}
	}
	return fNearest;
}

// AddBlastDecal(image, x, y, z, radius [, life [, opacity [, nx, ny, nz [, rays [, spin]]]]]): a scorch round a blast at x, y, z
// that lands on every surface within radius facing the blast, the floor and both walls of a corner alike, each taking the
// texture from the direction it faces most (triplanar), so none is stretched; it fades out towards the radius. nx, ny, nz
// is the normal of the surface hit (up by default): the centre is lifted off it by a quarter of the radius, or the surface
// itself would face across the centre and stay clean. opacity is 0 to 100, and life as AddProjectedDecal's. Unless rays
// is 0, short rays (up to 34, once) turn the decal square to the nearest wall and find the first surface along each of its
// six axes, so a floor under a slab or a wall behind a wall stays clean. They test the ground and the objects near it that
// take decals (not those SetEntityReceivesDecals refused, such as vehicles and soldiers), and are cast only when such an
// object's bounds come within their reach: the ground alone is left to the facing test, so a blast on open ground costs no
// rays.
// It is one of the projected decals: the same ids, limit and level clearing, and RemoveProjectedDecal,
// SetProjectedDecalOpacity, SetProjectedDecalGrass (the circle goes round x, y, z), SetProjectedDecalFacing (0.1 unless
// set, towards the centre) and SetProjectedDecalBlend work on it. spin turns it about the up axis, in degrees, when no wall
// squared it (on open ground), so scorches there don't all face the same way. Returns its id, 0 if the texture did not
// load, and the number of rays cast
int AddBlastDecal(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;
	const char* pImage = lua_tostring(L, 1);
	if (!pImage) return 0;
	float fHit[3] = { (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3), (float)lua_tonumber(L, 4) };
	float fRadius = lua_tonumber(L, 5);
	if (fRadius < 1.0f) fRadius = 1.0f;
	float fLife = -1, fOpacity = 1;
	if (n >= 6) { fLife = lua_tonumber(L, 6); if (fLife <= 0) fLife = -1; }
	if (n >= 7) fOpacity = lua_tonumber(L, 7) / 100.0f;
	float fN[3] = { 0, 1, 0 };
	if (n >= 10)
	{
		float fNX = lua_tonumber(L, 8), fNY = lua_tonumber(L, 9), fNZ = lua_tonumber(L, 10);
		float fLength = sqrtf(fNX * fNX + fNY * fNY + fNZ * fNZ);
		if (fLength > 0.000001f) { fN[0] = fNX / fLength; fN[1] = fNY / fLength; fN[2] = fNZ / fLength; }
	}
	bool bRays = (n < 11 || lua_tointeger(L, 11) != 0);
	float fSpin = (n >= 12) ? (float)lua_tonumber(L, 12) : 0.0f;

	// the centre, off the surface hit
	float fCentre[3] = { fHit[0] + fN[0] * fRadius * 0.25f, fHit[1] + fN[1] * fRadius * 0.25f, fHit[2] + fN[2] * fRadius * 0.25f };

	// the rays reach the radius along the box's axes, and those beside the first half the radius off to the side, so
	// all within 1.12 of it round the centre. They test only the objects in that cube that take decals (one that takes none
	// hides nothing and squares nothing); with none there they could only find the ground, a heightfield, whose far sides
	// the facing test already keeps clean, and none are cast
	int iRaysCast = 0;
	if (bRays)
	{
		float fReach = fRadius * 1.125f;
		float fMin[3] = { fCentre[0] - fReach, fCentre[1] - fReach, fCentre[2] - fReach };
		float fMax[3] = { fCentre[0] + fReach, fCentre[1] + fReach, fCentre[2] + fReach };
		if (WickedCall_FindDecalRayTargets(fMin, fMax) == 0) bRays = false;
	}

	// the box's axes: Y up, and X square to the wall hit, or else to the nearest wall round the centre, so a corner's
	// faces each meet one of its planes squarely; otherwise the world's
	float fAxes[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	const float fWorldDirs[4][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	float fWorldRays[4] = { 1, 1, 1, 1 };
	bool bWorldAxes = true;
	if (fabsf(fN[1]) < 0.7f)
	{
		float fLength = sqrtf(fN[0] * fN[0] + fN[2] * fN[2]);
		fAxes[0][0] = fN[0] / fLength; fAxes[0][1] = 0; fAxes[0][2] = fN[2] / fLength;
		bWorldAxes = false;
	}
	else if (bRays)
	{
		float fNearestWall = 1.0f;
		for (int i = 0; i < 4; i++)
		{
			float fWallN[3] = { 0, 0, 0 };
			fWorldRays[i] = BlastDecalRay(fCentre, fWorldDirs[i], fRadius, fWallN);
			iRaysCast++;
			float fAcross = sqrtf(fWallN[0] * fWallN[0] + fWallN[2] * fWallN[2]);
			if (fWorldRays[i] < fNearestWall && fabsf(fWallN[1]) < 0.7f && fAcross > 0.1f)
			{
				fNearestWall = fWorldRays[i];
				fAxes[0][0] = fWallN[0] / fAcross; fAxes[0][1] = 0; fAxes[0][2] = fWallN[2] / fAcross;
			}
		}
		if (fAxes[0][0] < 0.9999f) bWorldAxes = false;
	}
	// with no wall to square to, the spin turns the box about its Y; its own axis rays are then cast, as the world ones no
	// longer lie along it
	bool bSpun = false;
	if (bWorldAxes && fSpin != 0.0f)
	{
		float fSpinRad = fSpin * 0.0174532925f;
		fAxes[0][0] = cosf(fSpinRad); fAxes[0][1] = 0; fAxes[0][2] = sinf(fSpinRad);
		bSpun = true;
	}
	fAxes[2][0] = -fAxes[0][2]; fAxes[2][1] = 0; fAxes[2][2] = fAxes[0][0];

	// how far the first surface lies along each of the box's six axes, 5 bits each (+x, -x, +y, -y, +z, -z; 31 for none). A
	// surface counts only if four more rays beside the first, half the radius off either way along the other two axes,
	// all meet it within a twentieth of the radius: a flat face across the axis, not a post, a step or a slope
	uint32_t iOcclusion = ~0u;

	// the box's direction into the surface hit (-x for a wall, x being square to it; -y for a floor, +y for a ceiling): when
	// the rays find a flat face there where the hit was, a little behind it (10 units) everything is clipped, a room behind
	// a wall or the storey under a floor; a curved or sloped surface, whose rays disagree, is not clipped, as it falls away
	// behind the point hit
	const int iHitDirection = fabsf(fN[1]) < 0.7f ? 1 : (fN[1] > 0 ? 3 : 2);
	int iHitClipDirection = -1;
	float fHitClip = 0.0f;
	if (bRays)
	{
		iOcclusion = 0;
		for (int iAxis = 0; iAxis < 3; iAxis++)
		{
			for (int iSide = 0; iSide < 2; iSide++)
			{
				float fSign = iSide == 0 ? 1.0f : -1.0f;
				float fDir[3] = { fAxes[iAxis][0] * fSign, fAxes[iAxis][1] * fSign, fAxes[iAxis][2] * fSign };
				float fHitN[3] = { 0, 0, 0 };
				float fFirst;
				if (bWorldAxes && !bSpun && iAxis != 1 && fabsf(fN[1]) >= 0.7f)
					fFirst = fWorldRays[(iAxis == 0 ? 0 : 2) + iSide];
				else
				{
					fFirst = BlastDecalRay(fCentre, fDir, fRadius, fHitN);
					iRaysCast++;
				}
				uint32_t iCode = 31;
				if (fFirst < 1.0f)
				{
					float fNearest = fFirst, fFarthest = fFirst;
					bool bFlat = true;
					for (int iBeside = 0; iBeside < 4 && bFlat; iBeside++)
					{
						int iAcross = (iAxis + 1 + iBeside / 2) % 3;
						float fOffset = (iBeside & 1) ? -0.5f * fRadius : 0.5f * fRadius;
						float fFrom[3] = { fCentre[0] + fAxes[iAcross][0] * fOffset, fCentre[1] + fAxes[iAcross][1] * fOffset, fCentre[2] + fAxes[iAcross][2] * fOffset };
						float fAlong = BlastDecalRay(fFrom, fDir, fRadius, fHitN);
						iRaysCast++;
						if (fAlong >= 1.0f) bFlat = false;
						if (fAlong < fNearest) fNearest = fAlong;
						if (fAlong > fFarthest) fFarthest = fAlong;
					}
					if (bFlat && fFarthest - fNearest <= 0.05f)
					{
						iCode = (uint32_t)ceilf(fFarthest * 30.0f);
						if (iCode > 30) iCode = 30;
						if (iAxis * 2 + iSide == iHitDirection && fFarthest >= 0.1f && fFarthest <= 0.4f)
						{
							iHitClipDirection = iHitDirection;
							fHitClip = fFarthest + 10.0f / fRadius;
						}
					}
				}
				iOcclusion |= iCode << ((iAxis * 2 + iSide) * 5);
			}
		}
	}

	char pImagePath[MAX_PATH];
	strcpy_s(pImagePath, MAX_PATH, pImage);
	int iID = WickedCall_AddBlastDecal(pImagePath, fCentre[0], fCentre[1], fCentre[2], fRadius, &fAxes[0][0], iOcclusion, iHitClipDirection, fHitClip, fLife, fHit[0], fHit[1], fHit[2]);
	if (iID > 0 && fOpacity < 1.0f) WickedCall_SetProjectedDecalOpacity(iID, fOpacity);
	lua_pushinteger(L, iID);
	lua_pushinteger(L, iRaysCast);
	return 2;
}
// SetProjectedDecalBlend(id, power): how sharply a blast decal's surfaces take the texture from the direction they face
// most, 1 to 64 (8 unless set). Higher leaves fewer surfaces blending two or three directions (fewer samples, and no
// ghosted double image on a slope); too high shows a seam where the direction changes
int SetProjectedDecalBlend(lua_State *L)
{
	if (LUA_GETTOP(L) < 2) return 0;
	WickedCall_SetProjectedDecalBlend(lua_tointeger(L, 1), lua_tonumber(L, 2));
	return 0;
}
// RemoveProjectedDecal(id): removes it at once; -1 removes them all
int RemoveProjectedDecal(lua_State *L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	WickedCall_RemoveProjectedDecal(lua_tointeger(L, 1));
	return 0;
}
// SetProjectedDecalOpacity(id, percent): 0 to 100, for a fade of the script's own
int SetProjectedDecalOpacity(lua_State *L)
{
	if (LUA_GETTOP(L) < 2) return 0;
	WickedCall_SetProjectedDecalOpacity(lua_tointeger(L, 1), lua_tonumber(L, 2) / 100.0f);
	return 0;
}
// SetProjectedDecalFacing(id, cutoff [, fade]): only surfaces facing along the decal's normal (a blast decal's centre) by
// more than cutoff (a cosine, 0.2 unless set, 0.1 for a blast) take it, so it doesn't reach the inside face of a thin wall
// or streak a face along its box; -1 lets every surface in its box take it. It fades in over fade past the cutoff (0.01 to
// 1, 0.1 when left out), so a wider fade lets it fade out round a curve instead of ending in a line
int SetProjectedDecalFacing(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float fFade = 0.1f;
	if (n >= 3) fFade = lua_tonumber(L, 3);
	WickedCall_SetProjectedDecalFacing(lua_tointeger(L, 1), lua_tonumber(L, 2), fFade);
	return 0;
}
// SetProjectedDecalGrass(id, radius [, trees]): no grass blade is drawn within radius of the decal's centre (0 for none,
// the default). Those whose edge is nearest the camera (the one it stands in first) use the grass kill shapes that
// SetGrassKillBox's 8 leave free, 32 in all; one too far to reach the drawn grass takes none. trees
// 1 also removes every tree whose trunk reaches within radius of the centre (as far above and below), for the rest of the
// level: they stay gone when the decal does
int SetProjectedDecalGrass(lua_State *L)
{
	if (LUA_GETTOP(L) < 2) return 0;
	bool bTrees = (LUA_GETTOP(L) >= 3 && lua_tointeger(L, 3) != 0);
	WickedCall_SetProjectedDecalGrass(lua_tointeger(L, 1), lua_tonumber(L, 2), bTrees);
	return 0;
}
// SetProjectedDecalLimit(count): how many projected decals there can be, 48 unless set, up to 96 (they share the
// renderer's per-frame lights and matrices); the oldest go past it. Back to 48 at the next level
int SetProjectedDecalLimit(lua_State *L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	WickedCall_SetProjectedDecalLimit(lua_tointeger(L, 1));
	return 0;
}
int GetProjectedDecalLimit(lua_State *L)
{
	lua_pushinteger(L, WickedCall_GetProjectedDecalLimit());
	return 1;
}
int GetProjectedDecalCount(lua_State *L)
{
	lua_pushinteger(L, WickedCall_GetProjectedDecalCount());
	return 1;
}
// SetEntityReceivesDecals(e, on): 0 and the entity's model takes no projected decals, such as characters and vehicles
// passing over a scorch mark; every entity using that model shares its materials, so all of them change
int SetEntityReceivesDecals(lua_State *L)
{
	if (LUA_GETTOP(L) < 2) return 0;
	int e = lua_tointeger(L, 1);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	int iObj = t.entityelement[e].obj;
	if (iObj > 0 && ObjectExist(iObj) == 1) WickedCall_SetObjectReceivesDecals(GetObjectData(iObj), lua_tointeger(L, 2) != 0);
	return 0;
}
// SetEntityLOD(e, lod): the entity draws at LOD lod, 0 (full detail) to 3, whatever its distance; -1 (or leaving lod out)
// chooses by distance again. Only a model with LOD levels (its _lod.dbo) has any to force; a level past its last draws
// its last. Per entity, until the level ends
int SetEntityLOD(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int e = lua_tointeger(L, 1);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	int iLOD = (n >= 2) ? lua_tointeger(L, 2) : -1;
	int iObj = t.entityelement[e].obj;
	if (iObj > 0 && ObjectExist(iObj) == 1) WickedCall_SetObjectForceLOD(GetObjectData(iObj), iLOD);
	return 0;
}
// SetEntityShaderParam(e, n, value): a custom shader's parameter n, 1 to 7, as the .fpe's shader parameters set them;
// only materials with a custom shader have them. The model's materials are shared, so every entity using the model
// changes. Back to the model's own at the next level or when a test game ends
int SetEntityShaderParam(lua_State *L)
{
	if (LUA_GETTOP(L) < 3) return 0;
	int e = lua_tointeger(L, 1);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	int iObj = t.entityelement[e].obj;
	if (iObj > 0 && ObjectExist(iObj) == 1) WickedCall_SetObjectShaderParam(GetObjectData(iObj), lua_tointeger(L, 2), lua_tonumber(L, 3));
	return 0;
}
// SetEntityTextureScroll(e, du, dv): the entity's textures scroll by du, dv texture widths a second, the same at any
// frame rate (water in a channel, a conveyor belt); 0, 0 stops them where they are. The model's materials are shared, so
// every entity using the model scrolls. They go back to where they started at the next level or when a test game ends
int SetEntityTextureScroll(lua_State *L)
{
	if (LUA_GETTOP(L) < 3) return 0;
	int e = lua_tointeger(L, 1);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	int iObj = t.entityelement[e].obj;
	if (iObj > 0 && ObjectExist(iObj) == 1) WickedCall_SetObjectTextureScroll(GetObjectData(iObj), lua_tonumber(L, 2), lua_tonumber(L, 3));
	return 0;
}
int TriggerWaterSplash(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	g.decalx = lua_tonumber(L, 1);
	g.decaly = lua_tonumber(L, 2);
	g.decalz = lua_tonumber(L, 3);
	t.tInScale_f = lua_tonumber(L, 4);
	#ifdef WICKEDENGINE
	extern int g_iBlendMode;
	int storage = g_iBlendMode;
	g_iBlendMode = 5; // Additive.
	#endif
	decal_triggerwatersplash();
	#ifdef WICKEDENGINE
	g_iBlendMode = storage;
	#endif
}
int PlayFootfallSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 5 || n > 7 ) return 0;
	int footfalltype = lua_tonumber(L, 1);
	float fX = lua_tonumber(L, 2);
	float fY = lua_tonumber(L, 3);
	float fZ = lua_tonumber(L, 4);
	int lastfootfallsound = lua_tonumber(L, 5);
	bool bProceedToPlaySound = false;
	#ifdef WICKEDENGINE
	int iLeftOrRight = lua_tonumber(L, 6);
	int iWalkOrRun = lua_tonumber(L, 7);
	extern void sound_footfallsound_core(int, int, float, float, float, int, int, int*);
	sound_footfallsound_core(0, footfalltype, fX, fY, fZ, iLeftOrRight, iWalkOrRun, &lastfootfallsound);
	#else
	t.trndsnd = Rnd(3);
	if (t.trndsnd == 0) t.tsnd = t.material[footfalltype].tred0id;
	if (t.trndsnd == 1) t.tsnd = t.material[footfalltype].tred1id;
	if (t.trndsnd == 2) t.tsnd = t.material[footfalltype].tred2id;
	if (t.trndsnd == 3) t.tsnd = t.material[footfalltype].tred3id;
	if (t.tsnd > 0)
	{
		if (t.trndsnd == lastfootfallsound)
		{
			t.trndsnd = t.trndsnd + 1; if (t.trndsnd > 3) t.trndsnd = 0;
			if (t.trndsnd == 0) t.tsnd = t.material[footfalltype].tred0id;
			if (t.trndsnd == 1) t.tsnd = t.material[footfalltype].tred1id;
			if (t.trndsnd == 2) t.tsnd = t.material[footfalltype].tred2id;
			if (t.trndsnd == 3) t.tsnd = t.material[footfalltype].tred3id;
		}
		lastfootfallsound = t.trndsnd;
		// play this material sound (will play tsnd+0 through tsnd+4)
		t.tsoundtrigger = t.tsnd; t.tvol_f = 90;
		t.tspd_f = t.material[footfalltype].freq;
		t.tsx_f = fX;
		t.tsy_f = fY;
		t.tsz_f = fZ;
		material_triggersound (1);
	}
	#endif
	lua_pushnumber ( L, lastfootfallsound );
	return 1;
}
int ResetUnderwaterState ( lua_State *L )
{
	physics_player_reset_underwaterstate();
	return 0;
}
int SetUnderwaterOn ( lua_State *L )
{
	visuals_underwater_on();
	return 0;
}
int SetUnderwaterOff ( lua_State *L )
{
	visuals_underwater_off();
	return 0;
}
int SetWorldGravity(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 4)
		return 0;

	float x = lua_tonumber(L, 1);
	float y = lua_tonumber(L, 2);
	float z = lua_tonumber(L, 3);
	float fall = lua_tonumber(L, 4);

	ODESetWorldGravity(x, y, z, fall);
}

// Set Shader Values

int SetShaderVariable ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	int iShaderIndex = lua_tonumber(L, 1);
	char pConstantName[512];
	strcpy ( pConstantName, lua_tostring(L, 2) );
	float fValue1 = lua_tonumber(L, 3);
	float fValue2 = lua_tonumber(L, 4);
	float fValue3 = lua_tonumber(L, 5);
	float fValue4 = lua_tonumber(L, 6);
	int iShaderIndexStart = 1;
	int iShaderIndexFinish = 2000;
	if ( iShaderIndex > 0 ) { iShaderIndexStart = iShaderIndex; iShaderIndexFinish = iShaderIndex; }
	SetVector4 ( g.terrainvectorindex1, fValue1, fValue2, fValue3, fValue4 );
	for ( int iSI = iShaderIndexStart; iSI <= iShaderIndexFinish; iSI++ )
	{
		if ( GetEffectExist ( iSI ) == 1 ) 
		{
			DWORD pConstantPtr = GetEffectParameterIndex ( iSI, pConstantName );
			if ( pConstantPtr ) 
			{
				SetEffectConstantVEx( iSI, pConstantPtr, g.terrainvectorindex1 );
			}
		}
	}
	return 0;
}

//PE: Control cloud shader
void Wicked_Update_Cloud(void* visual);
// set when a script changes these, so a full visuals push applies them again (M-GridEditB.cpp LuaGameVisuals_Apply)
extern bool g_bLuaSetClouds, g_bLuaSetTreeWind, g_bLuaSetWind, g_bLuaSetWaterColor, g_bLuaSetWaterFog, g_bLuaSetLUT;
extern bool g_bLuaSetTreeSubsurface;
int GetCloudDensity(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudiness);
	return 1;
}
int SetCloudDensity(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudiness = lua_tonumber(L, 1);
	g_bLuaSetClouds = true;
	Wicked_Update_Cloud((void*) &t.gamevisuals);
	return 0;
}
int GetCloudCoverage(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudCoverage);
	return 1;
}
int SetCloudCoverage(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudCoverage = lua_tonumber(L, 1);
	g_bLuaSetClouds = true;
	Wicked_Update_Cloud((void*)&t.gamevisuals);
	return 0;
}
int GetCloudHeight(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudHeight);
	return 1;
}
int SetCloudHeight(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudHeight = lua_tonumber(L, 1);
	g_bLuaSetClouds = true;
	Wicked_Update_Cloud((void*)&t.gamevisuals);
	return 0;
}
int GetCloudThickness(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudThickness);
	return 1;
}
int SetCloudThickness(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudThickness = lua_tonumber(L, 1);
	g_bLuaSetClouds = true;
	Wicked_Update_Cloud((void*)&t.gamevisuals);
	return 0;
}
int GetCloudSpeed(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudSpeed);
	return 1;
}
int SetCloudSpeed(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudSpeed = lua_tonumber(L, 1);
	g_bLuaSetClouds = true;
	Wicked_Update_Cloud((void*)&t.gamevisuals);
	return 0;
}

//PE: Other Shaders
// SetTreeWind(amount [, speed]): the optional speed (0 to 12, 0 = tied to the amount as before) lets a storm sway
// trees far without also making them sway fast
int SetTreeWind(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.tree_wind = lua_tonumber(L, 1);
	//void WickedCall_UpdateTreeWind(float wind)
	WickedCall_UpdateTreeWind(t.gamevisuals.tree_wind);
	if (n >= 2)
	{
		t.gamevisuals.tree_wind_speed = max(0.0f, min(12.0f, (float)lua_tonumber(L, 2)));
		WickedCall_UpdateTreeWindSpeed(t.gamevisuals.tree_wind_speed);
	}
	g_bLuaSetTreeWind = true;
	return 0;
}
int GetTreeWind(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.tree_wind);
	lua_pushnumber(L, t.gamevisuals.tree_wind_speed);
	return 2;
}
// SetTreeSubsurface(v): how brightly the sun shines through the leaves of trees seen against it, 0 to 1 as the editor's
// Subsurface Scattering slider (full-detail trees and billboards). Kept through visuals pushes; the level's again at the
// next level and when a test game ends
int SetTreeSubsurface(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	t.gamevisuals.tree_sss = max(0.0f, min(1.0f, (float)lua_tonumber(L, 1)));
	WickedCall_UpdateTreeSubsurface(t.gamevisuals.tree_sss);
	g_bLuaSetTreeSubsurface = true;
	return 0;
}
int GetTreeSubsurface(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.tree_sss);
	return 1;
}


//Control Water Shader
//setter
int SetWaterHeight(lua_State *L) 
{
	t.terrain.waterliney_f = lua_tonumber(L, 1);
	terrain_updatewaterphysics();
	extern void WickedCall_UpdateWaterHeight(float);
	WickedCall_UpdateWaterHeight(t.terrain.waterliney_f);
	return 0;
}
int SetWaterShaderColor(lua_State *L) 
{
#ifdef WICKEDENGINE
	t.gamevisuals.WaterRed_f = lua_tonumber(L, 1);
	t.gamevisuals.WaterGreen_f = lua_tonumber(L, 2);
	t.gamevisuals.WaterBlue_f = lua_tonumber(L, 3);
	WickedCall_UpdateWaterColor(t.gamevisuals.WaterRed_f, t.gamevisuals.WaterGreen_f, t.gamevisuals.WaterBlue_f);
	g_bLuaSetWaterColor = true;
	return 0;
#else
	t.visuals.WaterRed_f = lua_tonumber(L, 1);
	t.visuals.WaterGreen_f = lua_tonumber(L, 2);
	t.visuals.WaterBlue_f = lua_tonumber(L, 3);
	SetVector4(g.terrainvectorindex, t.visuals.WaterRed_f / 256, t.visuals.WaterGreen_f / 256, t.visuals.WaterBlue_f / 256, 0);
	SetEffectConstantV(t.terrain.effectstartindex + 1, "WaterCol", g.terrainvectorindex);
	return 0;
#endif
}
int SetWaterWaveIntensity(lua_State *L)
{
	t.visuals.WaterWaveIntensity_f = lua_tonumber(L, 1);
	SetVector4(g.terrainvectorindex, t.visuals.WaterWaveIntensity_f, t.visuals.WaterWaveIntensity_f, 0, 0);
	SetEffectConstantV(t.terrain.effectstartindex + 1, "nWaterScale", g.terrainvectorindex);
	return 0;
}
// SetWaterFog(minDist, maxDist [, minAmount]): how clear the water is. Closer than minDist the water fog is at its
// minimum amount (0 to 1, lower = clearer), beyond maxDist the water is opaque; distances in world units, 0 to 100000,
// as the editor's water fog sliders. SetWaterTransparancy writes a legacy value the water no longer uses
int SetWaterFog(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float fMinDist = max(0.0f, min(100000.0f, (float)lua_tonumber(L, 1)));
	float fMaxDist = max(0.0f, min(100000.0f, (float)lua_tonumber(L, 2)));
	if (fMaxDist <= fMinDist) fMaxDist = fMinDist + 0.1f;
	// kept in t.gamevisuals like the water colour, so a test game's value never reaches the editor
	t.gamevisuals.WaterFogMinDist = fMinDist;
	t.gamevisuals.WaterFogMaxDist = fMaxDist;
	if (n >= 3) t.gamevisuals.WaterFogMinAmount = max(0.0f, min(1.0f, (float)lua_tonumber(L, 3)));
	extern void WickedCall_UpdateWaterFog(float fMinDist, float fMaxDist, float fMinAmount);
	WickedCall_UpdateWaterFog(t.gamevisuals.WaterFogMinDist, t.gamevisuals.WaterFogMaxDist, t.gamevisuals.WaterFogMinAmount);
	g_bLuaSetWaterFog = true;
	return 0;
}
int GetWaterFog(lua_State *L)
{
	lua_pushnumber(L, t.gamevisuals.WaterFogMinDist);
	lua_pushnumber(L, t.gamevisuals.WaterFogMaxDist);
	lua_pushnumber(L, t.gamevisuals.WaterFogMinAmount);
	return 3;
}
int SetWaterTransparancy(lua_State *L)
{
	t.visuals.WaterTransparancy_f = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "WaterTransparancy", t.visuals.WaterTransparancy_f);
	return 0;
}
int SetWaterReflection(lua_State *L)
{
	t.visuals.WaterReflection_f = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "WaterReflection", t.visuals.WaterReflection_f);
	return 0;
}
int SetWaterReflectionSparkleIntensity(lua_State *L)
{
	t.visuals.WaterReflectionSparkleIntensity = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "reflectionSparkleIntensity", t.visuals.WaterReflectionSparkleIntensity);
	return 0;
}
int SetWaterFlowDirection(lua_State *L)
{
	t.visuals.WaterFlowDirectionX = lua_tonumber(L, 1);
	t.visuals.WaterFlowDirectionY = lua_tonumber(L, 2);
	t.visuals.WaterFlowSpeed = lua_tonumber(L, 3);
	SetVector4(g.terrainvectorindex, t.visuals.WaterFlowDirectionX*t.visuals.WaterFlowSpeed, t.visuals.WaterFlowDirectionY*t.visuals.WaterFlowSpeed, 0, 0);
	SetEffectConstantV(t.terrain.effectstartindex + 1, "flowdirection", g.terrainvectorindex);
	return 0;
}
int SetWaterDistortionWaves(lua_State *L)
{
	t.visuals.WaterDistortionWaves = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "distortion2", t.visuals.WaterDistortionWaves);
	return 0;
}
int SetRippleWaterSpeed(lua_State *L)
{
	t.visuals.WaterSpeed1 = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "WaterSpeed1", t.visuals.WaterSpeed1);
	return 0;
}
//getter
int GetWaterHeight(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.terrain.waterliney_f);
	return 1;
}
int GetWaterWaveIntensity(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterWaveIntensity_f);
	return 1;
}
int GetWaterShaderColorRed(lua_State *L)
{
	lua = L;
#ifdef WICKEDENGINE
	lua_pushnumber(L, t.gamevisuals.WaterRed_f);
#else
	lua_pushnumber(L, t.visuals.WaterRed_f);
#endif
	return 1;
}
int GetWaterShaderColorGreen(lua_State *L)
{
	lua = L;
#ifdef WICKEDENGINE
	lua_pushnumber(L, t.gamevisuals.WaterGreen_f);
#else
	lua_pushnumber(L, t.visuals.WaterGreen_f);
#endif
	return 1;
}
int GetWaterShaderColorBlue(lua_State *L)
{
	lua = L;
#ifdef WICKEDENGINE
	lua_pushnumber(L, t.gamevisuals.WaterBlue_f);
#else
	lua_pushnumber(L, t.visuals.WaterBlue_f);
#endif
	return 1;
}
int GetWaterTransparancy(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterTransparancy_f);
	return 1;
}
int GetWaterReflection(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterReflection_f);
	return 1;
}
int GetWaterReflectionSparkleIntensity(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterReflectionSparkleIntensity);
	return 1;
}
int GetWaterFlowDirectionX(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterFlowDirectionX);
	return 1;
}
int GetWaterFlowDirectionY(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterFlowDirectionY);
	return 1;
}
int GetWaterFlowSpeed(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterFlowSpeed);
	return 1;
}
int GetWaterDistortionWaves(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterDistortionWaves);
	return 1;
}
int GetRippleWaterSpeed(lua_State *L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.WaterSpeed1);
	return 1;
}
#ifdef WICKEDENGINE
int GetWaterEnabled(lua_State* L)
{
	lua = L;
	lua_pushnumber(L, t.visuals.bWaterEnable);
	return 1;
}
#endif

int GetIsTestgame(lua_State *L) {
	lua = L;
	if ((t.game.gameisexe == 0 || g.gprofileinstandalone == 1) && t.game.runasmultiplayer == 0) 
	{
		lua_pushnumber(L, 1);
	}
	else 
	{
		lua_pushnumber(L, 0);
	}
	return 1;
}

//Dynamic sun.

int SetSunDirection(lua_State *L) 
{
	float fAx = lua_tonumber(L, 1);
	float fAy = lua_tonumber(L, 2);
	float fAz = lua_tonumber(L, 3);
	t.visuals.SunAngleX = fAx;
	t.visuals.SunAngleY = fAy;
	t.visuals.SunAngleZ = fAz;
	WickedCall_SetSunDirection(t.visuals.SunAngleX, t.visuals.SunAngleY, t.visuals.SunAngleZ);
	return 0;
}

#ifdef STORYBOARD

//Storyboaard
int FindLuaScreenNode(char* name);
int FindLuaScreenTitleNode(char* name);
int FindLuaScreenTitleNodeByKey(char* key);

int GetStoryboardActive(lua_State *L)
{
	lua = L;

	char pScreenName[512];
	strcpy(pScreenName, lua_tostring(L, 1));

	int iNode = FindLuaScreenNode(pScreenName);
	if (iNode >= 0 && strlen(Storyboard.gamename) > 0)
	{
		lua_pushnumber(L, 1);
	}
	else
	{
		lua_pushnumber(L, 0);
	}
	return 1;
}

int iSpecialLuaReturn = -1;
int SetScreenHUDGlobalScale(lua_State* L)
{
	float fGlobalScaleMod = lua_tonumber(L, 1);
	extern void screen_editor_setscalemod(float);
	screen_editor_setscalemod (fGlobalScaleMod);
	return 0;
}
int InitScreen(lua_State* L)
{
	// write to active LUA, i.e. g_UserGlobal[yourscript.user_variable_name]
	char pScreenName[512];
	strcpy(pScreenName, lua_tostring(L, 1));
	//int nodeid = FindLuaScreenNode(pScreenName);
	for (int nodeid = 0; nodeid < STORYBOARD_MAXNODES; nodeid++)
	{
		if (strlen(Storyboard.Nodes[nodeid].lua_name) > 0 && strnicmp(Storyboard.Nodes[nodeid].lua_name, "hud", 3) == NULL)
		{
			for (int i = 0; i < STORYBOARD_MAXWIDGETS; i++)
			{
				if (Storyboard.Nodes[nodeid].widget_type[i] == STORYBOARD_WIDGET_TEXT)
				{
					std::string readout = Storyboard.widget_readout[nodeid][i];
					if (stricmp(readout.c_str(), "User Defined Global") == NULL)
					{
						char pUserDefinedGlobal[256];
						sprintf(pUserDefinedGlobal, "g_UserGlobal['%s']", Storyboard.Nodes[nodeid].widget_label[i]);
						LuaSetInt(pUserDefinedGlobal, Storyboard.Nodes[nodeid].widget_initial_value[i]);
					}
					if (stricmp(readout.c_str(), "User Defined Global Text") == NULL)
					{
						char pUserDefinedGlobal[256];
						sprintf(pUserDefinedGlobal, "g_UserGlobal['%s']", Storyboard.Nodes[nodeid].widget_label[i]);
						//PE: We now reuse widget_click_sound for initial_value.
						LuaSetString(pUserDefinedGlobal, Storyboard.Nodes[nodeid].widget_click_sound[i]); // can we get from initial_value in screen editor?
					}
				}
			}
		}
	}
	return 0;
}
int DisplayScreen(lua_State* L)
{
	char pScreenName[512];
	strcpy(pScreenName, lua_tostring(L, 1));
	int screen_editor(int nodeid, bool standalone = false, char* screen = NULL);
	screen_editor(-1, true, pScreenName);
	lua_pushnumber(L, iSpecialLuaReturn);
	return 1;
}
int ForceMouseXYClick(lua_State* L)
{
	// called by cursorcontrol.lua so that when controller moves pointer in screen
	// the changes are carried into the engine so they can move the actual LUA mouse pointer and handle the click
	extern float LuaMousePosPercentX;
	extern float LuaMousePosPercentY;
	extern int LuaMouseClick;
	LuaMousePosPercentX = lua_tonumber(L, 1);
	LuaMousePosPercentY = lua_tonumber(L, 2);
	LuaMouseClick = lua_tonumber(L, 3);
	g.LUAMouseX = (GetDisplayWidth() / 100.0f) * LuaMousePosPercentX;
	g.LUAMouseY = (GetDisplayHeight() / 100.0f) * LuaMousePosPercentY;
	return 0;
}
int DisplayCurrentScreen(lua_State* L)
{
	int screen_editor(int nodeid, bool standalone = false, char* screen = NULL);
	if (t.game.activeStoryboardScreen >= 0) 
	{
		screen_editor(t.game.activeStoryboardScreen, true);
	}
	lua_pushnumber(L, iSpecialLuaReturn);
	return 1;
}
bool g_bEnableGunFireInHUD = false;
int DisableGunFireInHUD(lua_State* L)
{
	g_bEnableGunFireInHUD = false;
	return 0;
}
int EnableGunFireInHUD(lua_State* L)
{
	g_bEnableGunFireInHUD = true;
	return 0;
}
bool bDisableKeyToggles = false;
int DisableBoundHudKeys(lua_State* L)
{
	bDisableKeyToggles = true;
	return 0;
}
int EnableBoundHudKeys(lua_State* L)
{
	bDisableKeyToggles = false;
	return 0;
}
int CheckScreenToggles(lua_State* L)
{
	if (bDisableKeyToggles) return 0;
	extern void TriggerScreenFromKeyPress();
	TriggerScreenFromKeyPress();
	return 1;
}
int ScreenToggle(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L); if (n < 1) return 0;
	char pScreenTitle[512];
	strcpy(pScreenTitle, lua_tostring(L, 1));
	t.game.activeStoryboardScreen = -1;
	int nodeid = FindLuaScreenTitleNode(pScreenTitle);
	if (nodeid >= 0)
	{
		t.game.activeStoryboardScreen = nodeid;
	}
	return 0;
}
int ScreenToggleByKey(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L); if (n < 1) return 0;
	char pKeyToSearchFor[512];
	strcpy(pKeyToSearchFor, lua_tostring(L, 1));
	t.game.activeStoryboardScreen = -1;
	int nodeid = FindLuaScreenTitleNodeByKey(pKeyToSearchFor);
	if (nodeid >= 0)
	{
		t.game.activeStoryboardScreen = nodeid;
	}
	return 0;
}

int GetIfUsingTABScreen(lua_State* L)
{
	int iReturnValue = 0;
	if (g.tabmode > 0) iReturnValue = 1;
	lua_pushnumber(L, iReturnValue);
	return 1;
}

int GetCurrentScreen(lua_State* L)
{
	lua_pushnumber(L, t.game.activeStoryboardScreen);
	return 1;
}
int GetCurrentScreenName(lua_State* L)
{
	std::string check_lua_title = "";
	if (t.game.activeStoryboardScreen >= 0)
	{
		check_lua_title = Storyboard.Nodes[t.game.activeStoryboardScreen].title;
	}
	lua_pushstring(L, check_lua_title.c_str());
	return 1;
}

int GetScreenWidgetValue(lua_State *L)
{
	float fRet = -99999.0;
	char pScreenName[512];
	strcpy(pScreenName, lua_tostring(L, 1));
	int iButton = lua_tonumber(L, 2);
	if (iButton >= 0 && iButton < STORYBOARD_MAXOUTPUTS)
	{
		int nodeid = FindLuaScreenNode(pScreenName);
		if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
		{
			fRet = Storyboard.NodeSliderValues[nodeid][iButton];
		}
	}
	lua_pushnumber(L, fRet);
	return 1;
	//return 0;
}
int SetScreenWidgetValue(lua_State *L)
{
	float fRet = -99999.0;
	char pScreenName[512];
	strcpy(pScreenName, lua_tostring(L, 1));
	int iButton = lua_tonumber(L, 2);
	float fNewValue = lua_tonumber(L, 3);
	if (iButton >= 0 && iButton < STORYBOARD_MAXOUTPUTS)
	{
		int nodeid = FindLuaScreenNode(pScreenName);
		if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
		{
			fRet = Storyboard.NodeSliderValues[nodeid][iButton] = fNewValue;
		}
	}
	lua_pushnumber(L, fRet);
	return 1;
	//return 0;
}
int SetScreenWidgetSelection(lua_State *L)
{
	int iRet = -99999;
	char pScreenName[512];
	strcpy(pScreenName, lua_tostring(L, 1));
	int iButton = lua_tonumber(L, 2);
	if (iButton >= 0 && iButton < STORYBOARD_MAXOUTPUTS)
	{
		int nodeid = FindLuaScreenNode(pScreenName);
		if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
		{
			iRet = Storyboard.NodeRadioButtonSelected[nodeid] = iButton; // used by GRAPHICS SETTINGS (1,2,3)
		}
	}
	lua_pushnumber(L, iRet);
	return 1;
}
int GetScreenElementsType(lua_State* L)
{
	int iQty = 0;
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		char pReadoutName[512];
		strcpy(pReadoutName, lua_tostring(L, 1));
		if (strlen(pReadoutName) > 0)
		{
			int iPatternLength = strlen(pReadoutName) - 1;
			if (pReadoutName[iPatternLength] == '*')
			{
				// find pattern match with string passed in
				for (int n = 0; n < STORYBOARD_MAXWIDGETS; n++)
				{
					if (strnicmp(Storyboard.widget_readout[nodeid][n], pReadoutName, iPatternLength) == NULL)
					{
						iQty++;
					}
				}
			}
			else
			{
				// exact match
				for (int n = 0; n < STORYBOARD_MAXWIDGETS; n++)
				{
					if (stricmp(Storyboard.widget_readout[nodeid][n], pReadoutName) == NULL)
					{
						iQty++;
					}
				}
			}
		}
	}
	lua_pushnumber(L, iQty);
	return 1;
}
int GetScreenElementTypeID(lua_State* L)
{
	int iCount = 0;
	int iElementID = 0;
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		char pReadoutName[512];
		strcpy(pReadoutName, lua_tostring(L, 1));
		int iIndex = lua_tonumber(L, 2);
		for (int n = 0; n < STORYBOARD_MAXWIDGETS; n++)
		{
			int iPatternLength = strlen(pReadoutName) - 1;
			if (pReadoutName[iPatternLength] == '*')
			{
				// find pattern match with string passed in
				if (strnicmp(Storyboard.widget_readout[nodeid][n], pReadoutName, iPatternLength) == NULL)
				{
					iCount++;
					if (iCount == iIndex)
					{
						// found valid element from readout type match
						iElementID = 1 + n;

						// done
						break;
					}
				}
			}
			else
			{
				// exact match
				if (stricmp(Storyboard.widget_readout[nodeid][n], pReadoutName) == NULL)
				{
					iCount++;
					if (iCount == iIndex)
					{
						// found valid element from readout type match
						iElementID = 1 + n;

						// done
						break;
					}
				}
			}
		}
	}
	lua_pushnumber(L, iElementID);
	return 1;
}
int GetScreenElements(lua_State* L)
{
	int iQty = 0;
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		char pElementName[512];
		strcpy(pElementName, lua_tostring(L, 1));
		if (strlen(pElementName) > 0)
		{
			int iPatternLength = strlen(pElementName) - 1;
			if (pElementName[iPatternLength] == '*')
			{
				// find pattern match
				for (int n = 0; n < STORYBOARD_MAXWIDGETS; n++)
				{
					if (strnicmp(Storyboard.Nodes[nodeid].widget_label[n], pElementName, iPatternLength) == NULL)
					{
						iQty++;
					}
				}
			}
			else
			{
				// exact match
				for (int n = 0; n < STORYBOARD_MAXWIDGETS; n++)
				{
					if (stricmp(Storyboard.Nodes[nodeid].widget_label[n], pElementName) == NULL)
					{
						iQty++;
					}
				}
			}
		}
	}
	lua_pushnumber(L, iQty);
	return 1;
}
int GetScreenElementID(lua_State* L)
{
	int iCount = 0;
	int iElementID = 0;
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		char pElementName[512];
		strcpy(pElementName, lua_tostring(L, 1));
		int iIndex = lua_tonumber(L, 2);
		for (int n = 0; n < STORYBOARD_MAXWIDGETS; n++)
		{
			if (pElementName[0] == 's')
			{
				strcpy(pElementName, pElementName);
			}

			int iPatternLength = strlen(pElementName) - 1;
			if (pElementName[iPatternLength] == '*')
			{
				// find pattern match
				for (int n = 0; n < STORYBOARD_MAXWIDGETS; n++)
				{
					if (strnicmp(Storyboard.Nodes[nodeid].widget_label[n], pElementName, iPatternLength) == NULL)
					{
						iCount++;
						if (iCount == iIndex)
						{
							// found valid element
							iElementID = 1 + n;

							// done
							break;
						}
					}
				}
			}
			else
			{
				// exact match
				if (stricmp(Storyboard.Nodes[nodeid].widget_label[n], pElementName) == NULL)
				{
					iCount++;
					if (iCount == iIndex)
					{
						// found valid element
						iElementID = 1 + n;

						// done
						break;
					}
				}
			}
		}
	}
	lua_pushnumber(L, iElementID);
	return 1;
}
int GetScreenElementImage(lua_State* L)
{
	int iImgID = 0;
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		int iElementID = lua_tonumber(L, 1) - 1;
		if (iElementID >= 0 && iElementID < STORYBOARD_MAXWIDGETS)
		{
			iImgID = Storyboard.Nodes[nodeid].widget_normal_thumb_id[iElementID];
		}
	}
	lua_pushnumber(L, iImgID);
	return 1;
}
int GetScreenElementArea(lua_State* L)
{
	float fAreaX = 0;
	float fAreaY = 0;
	float fAreaWidth = 0;
	float fAreaHeight = 0;
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		int iElementID = lua_tonumber(L, 1) - 1;
		if (iElementID >= 0 && iElementID < STORYBOARD_MAXWIDGETS)
		{
			extern float screen_editor_scalemod (float);
			float fGlobalScale = screen_editor_scalemod((float)g_dwScreenWidth / 1920.0f);
			fAreaX = fabs(Storyboard.Nodes[nodeid].widget_pos[iElementID].x); // FABS to eliminate negative pos which is used to HIDE the widget
			fAreaY = fabs(Storyboard.Nodes[nodeid].widget_pos[iElementID].y); // FABS to eliminate negative pos which is used to HIDE the widget
			float widgetsizex = ImageWidth(Storyboard.Nodes[nodeid].widget_normal_thumb_id[iElementID]);
			float widgetsizey = ImageHeight(Storyboard.Nodes[nodeid].widget_normal_thumb_id[iElementID]);
			fAreaWidth = widgetsizex * fGlobalScale * Storyboard.Nodes[nodeid].widget_size[iElementID].x;
			fAreaHeight = widgetsizey * fGlobalScale * Storyboard.Nodes[nodeid].widget_size[iElementID].y;
			// convert to percentage system
			fAreaWidth = fabs(fAreaWidth / (float)g_dwScreenWidth) * 100.0f;
			fAreaHeight = fabs(fAreaHeight / (float)g_dwScreenHeight) * 100.0f;
			// position anchor is in middle of image!
			fAreaX -= fAreaWidth / 2.0f;
		}
	}
	lua_pushnumber(L, fAreaX);
	lua_pushnumber(L, fAreaY);
	lua_pushnumber(L, fAreaWidth);
	lua_pushnumber(L, fAreaHeight);
	return 4;
}
int GetScreenElementDetails(lua_State* L)
{
	float fRows = 0;
	float fColumns = 0;
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		int iElementID = lua_tonumber(L, 1) - 1;
		if (iElementID >= 0 && iElementID < STORYBOARD_MAXWIDGETS)
		{
			fRows = Storyboard.widget_textoffset[nodeid][iElementID].x;
			fColumns = Storyboard.widget_textoffset[nodeid][iElementID].y;
		}
	}
	lua_pushnumber(L, fRows);
	lua_pushnumber(L, fColumns);
	return 2;
}

int GetScreenElementName(lua_State* L)
{
	// prep return string
	char pReturnData[512];
	strcpy(pReturnData, "");
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		int iElementID = lua_tonumber(L, 1) - 1;
		if (iElementID >= 0 && iElementID < STORYBOARD_MAXWIDGETS)
		{
			strcpy(pReturnData, Storyboard.Nodes[nodeid].widget_label[iElementID]);
		}
	}
	lua_pushstring(L, pReturnData);
	return 1;
}
int SetScreenElementVisibility(lua_State* L)
{
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		int iElementID = lua_tonumber(L, 1) - 1;
		if (iElementID >= 0 && iElementID < STORYBOARD_MAXWIDGETS)
		{
			int iVisibility = lua_tonumber(L, 2);
			if ( iVisibility == 1 )
				Storyboard.widget_ingamehidden[nodeid][iElementID] = 0;
			else
				Storyboard.widget_ingamehidden[nodeid][iElementID] = 1;
		}
	}
	return 0;
}
int SetScreenElementPosition(lua_State* L)
{
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		int iElementID = lua_tonumber(L, 1) - 1;
		if (iElementID >= 0 && iElementID < STORYBOARD_MAXWIDGETS)
		{
			float fX = lua_tonumber(L, 2);
			float fY = lua_tonumber(L, 3);
			// and add back half of image width!
			float widgetsizex = ImageWidth(Storyboard.Nodes[nodeid].widget_normal_thumb_id[iElementID]);
			float fAreaWidth = widgetsizex * Storyboard.Nodes[nodeid].widget_size[iElementID].x;
			fAreaWidth = fabs(fAreaWidth / (float)g_dwScreenWidth) * 100.0f;
			fX += fAreaWidth / 2.0f;

			Storyboard.Nodes[nodeid].widget_pos[iElementID].x = fX;
			Storyboard.Nodes[nodeid].widget_pos[iElementID].y = fY;
		}
	}
	return 0;
}
int SetScreenElementText(lua_State* L)
{
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		int iElementID = lua_tonumber(L, 1) - 1;
		if (iElementID >= 0 && iElementID < STORYBOARD_MAXWIDGETS)
		{
			char* pLabel = (char*)lua_tostring(L, 2);
			char pUserDefinedGlobal[MAX_PATH];
			sprintf(pUserDefinedGlobal, "g_UserGlobal['%s']", Storyboard.Nodes[nodeid].widget_label[iElementID]);
			LuaSetString(pUserDefinedGlobal, pLabel);
		}
	}
	return 0;
}
int SetScreenElementColor(lua_State* L)
{
	int nodeid = t.game.activeStoryboardScreen;
	if (nodeid == -1) nodeid = t.game.ingameHUDScreen;
	if (nodeid >= 0 && nodeid < STORYBOARD_MAXNODES)
	{
		int iElementID = lua_tonumber(L, 1) - 1;
		if (iElementID >= 0 && iElementID < STORYBOARD_MAXWIDGETS)
		{
			float fX = lua_tonumber(L, 2);
			float fY = lua_tonumber(L, 3);
			float fZ = lua_tonumber(L, 4);
			float fW = lua_tonumber(L, 5);
			Storyboard.Nodes[nodeid].widget_font_color[iElementID].x = fX;
			Storyboard.Nodes[nodeid].widget_font_color[iElementID].y = fY;
			Storyboard.Nodes[nodeid].widget_font_color[iElementID].z = fZ;
			Storyboard.Nodes[nodeid].widget_font_color[iElementID].w = fW;
		}
	}
	return 0;
}

// Collection Items

int GetCollectionAttributeQuantity(lua_State* L)
{
	int iQty = g_collectionLabels.size();
	lua_pushnumber(L, iQty);
	return 1;
}
int GetCollectionAttributeLabel(lua_State* L)
{
	// prep return string
	char pReturnData[512];
	strcpy(pReturnData, "");

	// collection label
	int iCollectionLabelIndex = lua_tonumber(L, 1);
	if (iCollectionLabelIndex > 0 && iCollectionLabelIndex <= g_collectionLabels.size())
	{
		strcpy(pReturnData, g_collectionLabels[iCollectionLabelIndex-1].Get());
	}
	lua_pushstring(L, pReturnData);
	return 1;
}
int GetCollectionItemQuantity(lua_State* L)
{
	int iQty = g_collectionList.size();
	lua_pushnumber(L, iQty);
	return 1;
}
int GetCollectionItemAttribute(lua_State* L)
{
	// prep return string
	char pReturnData[512];
	strcpy(pReturnData, "");

	// which collection item
	int iCollectionListIndex = lua_tonumber(L, 1);
	if (iCollectionListIndex > 0 && iCollectionListIndex <= g_collectionList.size())
	{
		// find attribute label index
		int iLabelIndex = 0;
		char pAttributeLabel[512];
		strcpy(pAttributeLabel, lua_tostring(L, 2));
		for (iLabelIndex = 0; iLabelIndex < g_collectionLabels.size(); iLabelIndex++)
			if (stricmp(g_collectionLabels[iLabelIndex].Get(), pAttributeLabel) == NULL)
				break;

		// can pull field data from collection list item
		if (iLabelIndex < g_collectionList[iCollectionListIndex - 1].collectionFields.size())
		{
			strcpy(pReturnData, g_collectionList[iCollectionListIndex - 1].collectionFields[iLabelIndex].Get());
		}
	}
	lua_pushstring(L, pReturnData);
	return 1;
}

// Collection Quests

int GetCollectionQuestAttributeQuantity(lua_State* L)
{
	int iQty = g_collectionQuestLabels.size();
	lua_pushnumber(L, iQty);
	return 1;
}
int GetCollectionQuestAttributeLabel(lua_State* L)
{
	// collection label
	char pReturnData[512];
	strcpy(pReturnData, "");
	int iCollectionLabelIndex = lua_tonumber(L, 1);
	if (iCollectionLabelIndex > 0 && iCollectionLabelIndex <= g_collectionQuestLabels.size())
	{
		strcpy(pReturnData, g_collectionQuestLabels[iCollectionLabelIndex - 1].Get());
	}
	lua_pushstring(L, pReturnData);
	return 1;
}
int GetCollectionQuestQuantity(lua_State* L)
{
	int iQty = g_collectionQuestList.size();
	lua_pushnumber(L, iQty);
	return 1;
}
int GetCollectionQuestAttribute(lua_State* L)
{
	// which collection quest
	char pReturnData[512];
	strcpy(pReturnData, "");
	int iCollectionListIndex = lua_tonumber(L, 1);
	if (iCollectionListIndex > 0 && iCollectionListIndex <= g_collectionQuestList.size())
	{
		// find attribute label index
		int iLabelIndex = 0;
		char pAttributeLabel[512];
		strcpy(pAttributeLabel, lua_tostring(L, 2));
		for (iLabelIndex = 0; iLabelIndex < g_collectionQuestLabels.size(); iLabelIndex++)
			if (stricmp(g_collectionQuestLabels[iLabelIndex].Get(), pAttributeLabel) == NULL)
				break;

		// can pull field data from collection list quest
		if (iLabelIndex < g_collectionQuestList[iCollectionListIndex - 1].collectionFields.size())
		{
			strcpy(pReturnData, g_collectionQuestList[iCollectionListIndex - 1].collectionFields[iLabelIndex].Get());
		}
	}
	lua_pushstring(L, pReturnData);
	return 1;
}

// Inventory containers

int FindInventoryIndex (LPSTR pNameOfInventory)
{
	int bothplayercontainers = -1;
	for (int n = 0; n < t.inventoryContainers.size(); n++)
	{
		if (stricmp(t.inventoryContainers[n].Get(), pNameOfInventory) == NULL)
		{
			bothplayercontainers = n;
			break;
		}
	}
	return bothplayercontainers;
}
int MakeInventoryContainer (lua_State* L)
{
	int containerindex = -1;
	char pNameOfInventory[512];
	strcpy(pNameOfInventory, lua_tostring(L, 1));
	for (int n = 0; n < t.inventoryContainers.size(); n++)
	{
		if (stricmp(t.inventoryContainers[n].Get(), pNameOfInventory) == NULL)
		{
			containerindex = n;
			break;
		}
	}
	if (containerindex == -1)
	{
		// create new container
		t.inventoryContainers.push_back(pNameOfInventory);
		containerindex = t.inventoryContainers.size() - 1;
		t.inventoryContainer[containerindex].clear();
	}
	lua_pushnumber(L, containerindex);
	return 1;
}
int GetInventoryTotal(lua_State* L)
{
	int iQty = t.inventoryContainers.size();
	lua_pushnumber(L, iQty);
	return 1;
}
int GetInventoryName(lua_State* L)
{
	int iInventoryIndex = lua_tonumber(L, 1);
	char pNameOfInventory[512];
	strcpy(pNameOfInventory, "");
	if (iInventoryIndex >= 0 && iInventoryIndex < t.inventoryContainers.size())
		strcpy(pNameOfInventory, t.inventoryContainers[iInventoryIndex].Get());
	lua_pushstring(L, pNameOfInventory);
	return 1;
}
int GetInventoryExist(lua_State* L)
{
	int iExist = 0;
	t.game.activeStoryboardScreen = -1;
	LPSTR pScreenTitle = "HUD Screen 2"; // traditionally the template RPG INVENTORY SCREEN!
	int nodeid = FindLuaScreenTitleNode(pScreenTitle);
	if (nodeid >= 0)
	{
		char pNameOfInventory[512];
		strcpy(pNameOfInventory, lua_tostring(L, 1));
		int bothplayercontainers = FindInventoryIndex(pNameOfInventory);
		if (bothplayercontainers >= 0) iExist = 1;
	}
	lua_pushnumber(L, iExist);
	return 1;
}
int GetInventoryQuantity(lua_State* L)
{
	int iQty = 0;
	char pNameOfInventory[512];
	strcpy(pNameOfInventory, lua_tostring(L, 1));
	int bothplayercontainers = FindInventoryIndex(pNameOfInventory);
	if (bothplayercontainers >= 0)
	{
		iQty = t.inventoryContainer[bothplayercontainers].size();
	}
	lua_pushnumber(L, iQty);
	return 1;
}
int GetInventoryItem(lua_State* L)
{
	int iCollectionItemID = 0;
	char pNameOfInventory[512];
	strcpy(pNameOfInventory, lua_tostring(L, 1));
	int bothplayercontainers = FindInventoryIndex(pNameOfInventory);
	if (bothplayercontainers >= 0)
	{
		int iInventoryIndex = lua_tonumber(L, 2);
		if (iInventoryIndex > 0 && iInventoryIndex <= t.inventoryContainer[bothplayercontainers].size())
		{
			iCollectionItemID = t.inventoryContainer[bothplayercontainers][iInventoryIndex - 1].collectionID;
		}
	}
	lua_pushnumber(L, iCollectionItemID);
	return 1;
}
int GetInventoryItemID(lua_State* L)
{
	int iItemEntityID = 0;
	char pNameOfInventory[512];
	strcpy(pNameOfInventory, lua_tostring(L, 1));
	int bothplayercontainers = FindInventoryIndex(pNameOfInventory);
	if (bothplayercontainers >= 0)
	{
		int iInventoryIndex = lua_tonumber(L, 2);
		if (iInventoryIndex > 0 && iInventoryIndex <= t.inventoryContainer[bothplayercontainers].size())
		{
			iItemEntityID = t.inventoryContainer[bothplayercontainers][iInventoryIndex - 1].e;
		}
	}
	lua_pushnumber(L, iItemEntityID);
	return 1;
}
int GetInventoryItemSlot(lua_State* L)
{
	int iItemSlot = 0;
	char pNameOfInventory[512];
	strcpy(pNameOfInventory, lua_tostring(L, 1));
	int bothplayercontainers = FindInventoryIndex(pNameOfInventory);
	if (bothplayercontainers >= 0)
	{
		int iInventoryIndex = lua_tonumber(L, 2);
		if (iInventoryIndex > 0 && iInventoryIndex <= t.inventoryContainer[bothplayercontainers].size())
		{
			iItemSlot = t.inventoryContainer[bothplayercontainers][iInventoryIndex - 1].slot;
		}
	}
	lua_pushnumber(L, iItemSlot);
	return 1;
}
int SetInventoryItemSlot(lua_State* L)
{
	char pNameOfInventory[512];
	strcpy(pNameOfInventory, lua_tostring(L, 1));
	int bothplayercontainers = FindInventoryIndex(pNameOfInventory);
	if (bothplayercontainers >= 0)
	{
		int iInventoryIndex = lua_tonumber(L, 2);
		if (iInventoryIndex > 0 && iInventoryIndex <= t.inventoryContainer[bothplayercontainers].size())
		{
			int iNewSlotIndex = lua_tonumber(L, 3);
			t.inventoryContainer[bothplayercontainers][iInventoryIndex - 1].slot = iNewSlotIndex;
		}
	}
	return 0;
}

void darklua_refreshhaskeystatefor(LPSTR keyobjectname)
{
	// all checks case insensitive
	cstr checkthisname = Lower(keyobjectname);

	// called when a collected state changes (i.e. a key) but need to allow all doors (haskey users) to refresh without messing up other haskey states
	for (int e = 1; e <= g.entityelementlist; e++)
	{
		//  check if demilited key
		t.masterkeyname_s = Lower(t.entityelement[e].eleprof.usekey_s.Get());
		if (Len(t.masterkeyname_s.Get()) > 0)
		{
			t.tmultikey = 0;
			for (int n = 1; n <= Len(t.masterkeyname_s.Get()); n++)
			{
				if (cstr(Mid(t.masterkeyname_s.Get(), n)) == ";")
				{
					t.tmultikey = 1;
				}
			}
			//  Is USEKEY Collected?
			bool bNeedToRefreshDoor = false;
			if (t.tmultikey == 0)
			{
				//  (SINGLE)
				for (int te = 1; te <= g.entityelementlist; te++)
				{
					if (stricmp(checkthisname.Get(), Lower(t.entityelement[te].eleprof.name_s.Get())) == NULL)
					{
						if (cstr(Lower(t.entityelement[te].eleprof.name_s.Get())) == t.masterkeyname_s)
						{
							bNeedToRefreshDoor = true; break;
						}
					}
				}
			}
			else
			{
				//  (MULTIPLE)
				int n = 1;
				while (n <= Len(t.masterkeyname_s.Get()))
				{
					t.keyname_s = "";
					while (n <= Len(t.masterkeyname_s.Get()))
					{
						if (cstr(Mid(t.masterkeyname_s.Get(), n)) == ";")  break;
						t.keyname_s = t.keyname_s + Mid(t.masterkeyname_s.Get(), n);
						++n;
					}
					//  look for this key
					int ttokay = 0;
					for (int te = 1; te <= g.entityelementlist; te++)
					{
						if (stricmp(checkthisname.Get(), Lower(t.entityelement[te].eleprof.name_s.Get())) == NULL)
						{
							if (cstr(Lower(t.entityelement[te].eleprof.name_s.Get())) == t.keyname_s)
							{
								ttokay = 1; break;
							}
						}
					}
					//  any key not found means overall master key not valid
					if (ttokay == 0)  bNeedToRefreshDoor = true;
					++n;
				}
			}
			if (bNeedToRefreshDoor == true)
			{
				t.entityelement[e].lua.haskey = 0;
			}
		}
	}
}
int MoveInventoryItem (lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;
	char pNameOfInventoryFrom[512];
	strcpy(pNameOfInventoryFrom, lua_tostring(L, 1));
	int collectionindex = lua_tonumber(L, 3);
	int entityindex = lua_tonumber(L, 4);
	int bothplayercontainersfrom = FindInventoryIndex(pNameOfInventoryFrom);
	if (bothplayercontainersfrom >= 0)
	{
		bool bNeedToRecreateContainerFrom = false;
		char pNameOfInventoryTo[512];
		strcpy(pNameOfInventoryTo, lua_tostring(L, 2));
		int bothplayercontainersto = FindInventoryIndex(pNameOfInventoryTo);
		if (bothplayercontainersto >= 0)
		{
			int slotindex = lua_tonumber(L, 5);
			if (bothplayercontainersfrom == bothplayercontainersto)
			{
				// moved within same container
				int iListSize = t.inventoryContainer[bothplayercontainersfrom].size();
				for (int n = 0; n < iListSize; n++)
				{
					if (slotindex != -1)
					{
						if (collectionindex == -1)
						{
							if (t.inventoryContainer[bothplayercontainersfrom][n].e == entityindex)
							{
								t.inventoryContainer[bothplayercontainersfrom][n].slot = slotindex;
								break;
							}
						}
						else
						{
							if (t.inventoryContainer[bothplayercontainersfrom][n].collectionID == collectionindex)
							{
								t.inventoryContainer[bothplayercontainersfrom][n].slot = slotindex;
								break;
							}
						}
					}
				}
			}
			else
			{
				// different containers
				int iListSize = t.inventoryContainer[bothplayercontainersfrom].size();
				for (int n = 0; n < iListSize; n++)
				{
					bool bMatch = false;
					if (collectionindex == -1)
					{
						if (t.inventoryContainer[bothplayercontainersfrom][n].e == entityindex)
							bMatch = true;
					}
					else
					{
						if (t.inventoryContainer[bothplayercontainersfrom][n].collectionID == collectionindex)
							bMatch = true;
					}
					if (bMatch==true)
					{
						// create item
						inventoryContainerType item;
						item.e = t.inventoryContainer[bothplayercontainersfrom][n].e;
						item.collectionID = t.inventoryContainer[bothplayercontainersfrom][n].collectionID;
						item.slot = slotindex;
						if (item.slot == -1)
						{
							item.slot = 0;
							while (1)
							{
								bool bCanIUseThisSlot = true;
								for (int nn = 0; nn < t.inventoryContainer[bothplayercontainersto].size(); nn++)
								{
									if (t.inventoryContainer[bothplayercontainersto][nn].slot == item.slot)
									{
										bCanIUseThisSlot = false;
										break;
									}
								}
								if (bCanIUseThisSlot == false)
									item.slot++;
								else
									break;
							}
						}
						// remove from old (below)
						t.inventoryContainer[bothplayercontainersfrom][n].e = -2;
						bNeedToRecreateContainerFrom = true;
						// add new item or increase resource quantity
						bool bWeAddedToQuantityOfAnother = false;
						if ( item.e > 0 && t.entityelement[item.e].eleprof.iscollectable == 2)
						{
							int iListToSize = t.inventoryContainer[bothplayercontainersto].size();
							for (int n = 0; n < iListToSize; n++)
							{
								if (t.inventoryContainer[bothplayercontainersto][n].collectionID == item.collectionID)
								{
									// can add to resource, no need to create new item in dest
									int ee = t.inventoryContainer[bothplayercontainersto][n].e;
									if (ee > 0)
									{
										int iQtyToAdd = t.entityelement[item.e].eleprof.quantity;
										if (iQtyToAdd < 1) iQtyToAdd = 1;
										t.entityelement[ee].eleprof.quantity += iQtyToAdd;
										t.entityelement[item.e].eleprof.quantity = 0;
										bWeAddedToQuantityOfAnother = true;
										break;
									}
								}
							}
						}
						if (bWeAddedToQuantityOfAnother == false )
						{
							// add new item to dest
							t.inventoryContainer[bothplayercontainersto].push_back(item);
						}
						// ensure activate and deactivate entity as it passes from player/hotkeys to shop/chest/etc
						if (item.e > 0)
						{
							if (bothplayercontainersto == 0 || bothplayercontainersto == 1)
							{
								if (bothplayercontainersto == 0) t.entityelement[item.e].collected = 1;
								if (bothplayercontainersto == 1) t.entityelement[item.e].collected = 2;
								darklua_refreshhaskeystatefor(t.entityelement[item.e].eleprof.name_s.Get());
								t.entityelement[item.e].active = 1;
							}
							else
							{
								t.entityelement[item.e].active = 0;
								t.entityelement[item.e].collected = 0;
								darklua_refreshhaskeystatefor(t.entityelement[item.e].eleprof.name_s.Get());
							}
						}
						break;
					}
				}
			}
		}
		else
		{
			// moving item to limbo (for resources that hit zero quantity)
			int iListSize = t.inventoryContainer[bothplayercontainersfrom].size();
			for (int n = 0; n < iListSize; n++)
			{
				bool bMatch = false;
				if (collectionindex == -1)
				{
					if (t.inventoryContainer[bothplayercontainersfrom][n].e == entityindex)
						bMatch = true;
				}
				else
				{
					if (t.inventoryContainer[bothplayercontainersfrom][n].collectionID == collectionindex)
						bMatch = true;
				}
				if (bMatch == true)
				{
					// remove (below)
					t.inventoryContainer[bothplayercontainersfrom][n].e = -2;
					bNeedToRecreateContainerFrom = true;
					break;
				}
			}
		}
		if ( bNeedToRecreateContainerFrom == true)
		{
			std::vector <inventoryContainerType> inventoryContainerTemp;
			inventoryContainerTemp.clear();
			int iListSize = t.inventoryContainer[bothplayercontainersfrom].size();
			for (int n = 0; n < iListSize; n++)
			{
				if (t.inventoryContainer[bothplayercontainersfrom][n].e != -2)
				{
					inventoryContainerTemp.push_back(t.inventoryContainer[bothplayercontainersfrom][n]);
				}
			}
			t.inventoryContainer[bothplayercontainersfrom] = inventoryContainerTemp;
		}
	}
	return 0;
}
int DeleteAllInventoryContainers (lua_State* L)
{
	for (int c = 0; c < t.inventoryContainers.size(); c++)
	{
		t.inventoryContainer[c].clear();
	}
	t.inventoryContainers.clear();
	return 0;
}
int AddInventoryItem (lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	char pNameOfInventoryTo[512];
	strcpy(pNameOfInventoryTo, lua_tostring(L, 1));
	int bothplayercontainersto = FindInventoryIndex(pNameOfInventoryTo);
	if (bothplayercontainersto >= 0)
	{
		int collectionindex = lua_tonumber(L, 2);
		int newe = lua_tonumber(L, 3);
		int slotindex = lua_tonumber(L, 4);
		inventoryContainerType item;
		item.e = newe;
		item.collectionID = collectionindex;
		item.slot = slotindex;
		t.inventoryContainer[bothplayercontainersto].push_back(item);
		// ensure activate and deactivate entity as it passes from player/hotkeys to shop/chest/etc
		if (bothplayercontainersto == 0 || bothplayercontainersto == 1)
			t.entityelement[item.e].active = 1;
		else
			t.entityelement[item.e].active = 0;
	}
	return 0;
}

#endif

// Game Player Control/State Set/Get commands

int SetGamePlayerControlData ( lua_State *L, int iDataMode )
{
	lua = L;
	int iSrc = 0;
	int iDest = 0;
	int n = LUA_GETTOP(L);
	if ( iDataMode < 500 )
	{
		if ( n < 1 ) return 0;
	}
	else
	{
		if ( n < 2 ) return 0;
	}
	int gunId = t.gunid;
	int fireMode = g.firemode;
	int param = 1;
	if ( n > 1 && iDataMode > 200 && iDataMode < 500 ) 
	{
		gunId = lua_tonumber( L, 1 );
		if ( n == 2 )
		{
			fireMode = 0;
			param = 2;
		}
		else
		{
			fireMode = lua_tonumber( L, 2 );
			param = 3;
		}
	}
	switch ( iDataMode )
	{
		case 1 : t.playercontrol.jetpackmode = lua_tonumber(L, 1); break;
		case 2 : t.playercontrol.jetpackfuel_f = lua_tonumber(L, 1); break;
		case 3 : t.playercontrol.jetpackhidden = lua_tonumber(L, 1); break;
		case 4 : t.playercontrol.jetpackcollected = lua_tonumber(L, 1); break;
		case 5 : t.playercontrol.soundstartindex = lua_tonumber(L, 1); break;
		case 6 : t.playercontrol.jetpackparticleemitterindex = lua_tonumber(L, 1); break;
		case 7 : t.playercontrol.jetpackthrust_f = lua_tonumber(L, 1); break;
		case 8 : t.playercontrol.startstrength = lua_tonumber(L, 1); break;

		case 9 : t.playercontrol.isrunning = lua_tonumber(L, 1); break;
		case 10 : break;
		case 11 : t.playercontrol.cx_f = lua_tonumber(L, 1); break;
		case 12 : t.playercontrol.cy_f = lua_tonumber(L, 1); break;
		case 13 : t.playercontrol.cz_f = lua_tonumber(L, 1); break;
		case 14 : 
		{
#ifdef FASTBULLETPHYSICS
			t.playercontrol.basespeed_f *= 2.0; //FASTBULLETPHYSICS need to move faster.
#else
			t.playercontrol.basespeed_f = lua_tonumber(L, 1);
			extern bool bPhysicsRunningAt120FPS;
			if (!bPhysicsRunningAt120FPS)
				t.playercontrol.basespeed_f *= 2.0;
#endif
			break;
		}
		case 15 : t.playercontrol.canrun = lua_tonumber(L, 1); break;
		case 16 : t.playercontrol.maxspeed_f = lua_tonumber(L, 1); break;
		case 17 : t.playercontrol.topspeed_f = lua_tonumber(L, 1); break;
		case 18 : t.playercontrol.movement = lua_tonumber(L, 1); break;
		case 19 : t.playercontrol.movey_f = lua_tonumber(L, 1); break;
		case 20 : t.playercontrol.lastmovement = lua_tonumber(L, 1); break;
		case 21 : t.playercontrol.footfallcount = lua_tonumber(L, 1); break;
		case 22 : break;
		case 23 : t.playercontrol.gravityactive = lua_tonumber(L, 1); break;
		case 24 : t.playercontrol.plrhitfloormaterial = lua_tonumber(L, 1); break;
		case 25:  t.playercontrol.underwater = lua_tonumber(L, 1); break;
		case 26 : t.playercontrol.jumpmode = lua_tonumber(L, 1); break;
		case 27 : t.playercontrol.jumpmodecanaffectvelocitycountdown_f = lua_tonumber(L, 1); break;
		case 28 : t.playercontrol.speed_f = lua_tonumber(L, 1); break;
		case 29 : t.playercontrol.accel_f = lua_tonumber(L, 1); break;
		case 30 : t.playercontrol.speedratio_f = lua_tonumber(L, 1); break;
		case 31 : t.playercontrol.wobble_f = lua_tonumber(L, 1); break;
		case 32 : t.playercontrol.wobblespeed_f = lua_tonumber(L, 1); break;
		case 33 : t.playercontrol.wobbleheight_f = lua_tonumber(L, 1); break;
		case 34 : t.playercontrol.jumpmax_f = lua_tonumber(L, 1); break;
		case 35 : t.playercontrol.pushangle_f = lua_tonumber(L, 1); break;
		case 36 : t.playercontrol.pushforce_f = lua_tonumber(L, 1); break;
		case 37 : t.playercontrol.footfallpace_f = lua_tonumber(L, 1); break;
		case 38 : t.playercontrol.lockatheight = lua_tonumber(L, 1); break;
		case 39 : t.playercontrol.controlheight = lua_tonumber(L, 1); break;
		case 40 : t.playercontrol.controlheightcooldown = lua_tonumber(L, 1); break;
		case 41 : t.playercontrol.storemovey = lua_tonumber(L, 1); break;
		case 42 : break;
		case 43 : t.playercontrol.hurtfall = lua_tonumber(L, 1); break;
		case 44 : t.playercontrol.leanoverangle_f = lua_tonumber(L, 1); break;
		case 45 : t.playercontrol.leanover_f = lua_tonumber(L, 1); break;
		case 46 : 
			t.playercontrol.camerashake_f = lua_tonumber(L, 1); 
			//#ifdef WICKEDENGINE
			//if (t.playercontrol.camerashake_f > 25.0f)  t.playercontrol.camerashake_f = 25.0f;
			//#endif
			break;
		case 47 : t.playercontrol.finalcameraanglex_f = lua_tonumber(L, 1); break;
		case 48 : t.playercontrol.finalcameraangley_f = lua_tonumber(L, 1); break;
		case 49 : t.playercontrol.finalcameraanglez_f = lua_tonumber(L, 1); break;
		case 50 : t.playercontrol.camrightmousemode = lua_tonumber(L, 1); break;
		case 51 : t.playercontrol.camcollisionsmooth = lua_tonumber(L, 1); break;
		case 52 : t.playercontrol.camcurrentdistance = lua_tonumber(L, 1); break;
		case 53 : t.playercontrol.camdofullraycheck = lua_tonumber(L, 1); break;
		case 54 : t.playercontrol.lastgoodcx_f = lua_tonumber(L, 1); break;
		case 55 : t.playercontrol.lastgoodcy_f = lua_tonumber(L, 1); break;
		case 56 : t.playercontrol.lastgoodcz_f = lua_tonumber(L, 1); break;
		case 57 : break;
		case 58 : t.playercontrol.flinchx_f = lua_tonumber(L, 1); break;
		case 59 : t.playercontrol.flinchy_f = lua_tonumber(L, 1); break;
		case 60 : t.playercontrol.flinchz_f = lua_tonumber(L, 1); break;
		case 61 : t.playercontrol.flinchcurrentx_f = lua_tonumber(L, 1); break;
		case 62 : t.playercontrol.flinchcurrenty_f = lua_tonumber(L, 1); break;
		case 63 : t.playercontrol.flinchcurrentz_f = lua_tonumber(L, 1); break;
		case 64 : t.playercontrol.footfalltype = lua_tonumber(L, 1); break;
		case 65 : t.playercontrol.ripplecount_f = lua_tonumber(L, 1); break;
		case 66 : t.playercontrol.lastfootfallsound = lua_tonumber(L, 1); break;
		case 67 : t.playercontrol.inwaterstate = lua_tonumber(L, 1); break;
		case 68 : t.playercontrol.drowntimestamp = lua_tonumber(L, 1); break;
		case 69 : t.playercontrol.deadtime = lua_tonumber(L, 1); break;
		case 70 : t.playercontrol.swimtimestamp = lua_tonumber(L, 1); break;
		case 71 : t.playercontrol.redDeathFog_f = lua_tonumber(L, 1); break;
		case 72 : t.playercontrol.heartbeatTimeStamp = lua_tonumber(L, 1); break;
		case 81 : t.playercontrol.thirdperson.enabled = lua_tonumber(L, 1); break;
		case 82 : t.playercontrol.thirdperson.characterindex = lua_tonumber(L, 1); break;
		case 83 : t.playercontrol.thirdperson.camerafollow = lua_tonumber(L, 1); break;
		case 84 : t.playercontrol.thirdperson.camerafocus = lua_tonumber(L, 1); break;
		case 85 : t.playercontrol.thirdperson.charactere = lua_tonumber(L, 1); break;
		case 86 : break;
		case 87 : t.playercontrol.thirdperson.shotfired = lua_tonumber(L, 1); break;
		case 88 : t.playercontrol.thirdperson.cameradistance = lua_tonumber(L, 1); break;
		case 89 : t.playercontrol.thirdperson.cameraspeed = lua_tonumber(L, 1); break;
		case 90 : t.playercontrol.thirdperson.cameralocked = lua_tonumber(L, 1); break;
		case 91 : t.playercontrol.thirdperson.cameraheight = lua_tonumber(L, 1); break;
		case 92 : t.playercontrol.thirdperson.camerashoulder = lua_tonumber(L, 1); break;

		case 99 : break;
		case 101 : t.gunmode = lua_tonumber(L, 1); break;
		case 102 : t.player[t.plrid].state.firingmode = lua_tonumber(L, 1); break;
		case 103 : g.weaponammoindex = lua_tonumber(L, 1); break;
		case 104 : g.ammooffset = lua_tonumber(L, 1); break;
		case 105 : g.ggunmeleekey = lua_tonumber(L, 1); break;
		case 106 : t.player[t.plrid].state.blockingaction = lua_tonumber(L, 1); break;
		case 107 : t.gunshootnoammo = lua_tonumber(L, 1); break;
		case 108 : g.playerunderwater = lua_tonumber(L, 1); break;
		case 109 : g.gdisablerightmousehold = lua_tonumber(L, 1); break;
		case 110 : g.gxbox = lua_tonumber(L, 1); break;
		case 111 : break;
		case 112 : break;
		case 113 : break;
		case 114 : t.gunzoommode = lua_tonumber(L, 1); break;
		case 115 : t.gunzoommag_f = lua_tonumber(L, 1); break;
		case 116 : t.gunreloadnoammo = lua_tonumber(L, 1); break;
		case 117 : g.plrreloading = lua_tonumber(L, 1); break;
		case 118 : g.ggunaltswapkey1 = lua_tonumber(L, 1); break;
		case 119 : g.ggunaltswapkey2 = lua_tonumber(L, 1); break;
		case 120 : 
		{
			t.weaponkeyselection = lua_tonumber(L, 1);
			// Trigger zoom out so that the player doesn't stay zoomed in when picking up a new weapon
			if (t.gunzoommode == 9 || t.gunzoommode == 10)
			{
				t.gunzoommode = 11;
			}
		}
		break;

		case 121 : t.weaponindex = lua_tonumber(L, 1); break;
		case 122 : t.player[t.plrid].command.newweapon = lua_tonumber(L, 1); break;
		case 123 : t.gunid = lua_tonumber(L, 1); break;
		case 124 : t.gunselectionafterhide = lua_tonumber(L, 1); break;
		case 125 : t.gunburst = lua_tonumber(L, 1); break;
		case 126 : break;
		case 127 : break;
		case 128 : break;
		case 129 : t.plrkeyForceKeystate = lua_tonumber(L, 1); break;
		case 130 : t.plrzoominchange = lua_tonumber(L, 1); break;
		case 131 : t.plrzoomin_f = lua_tonumber(L, 1); break;
		case 132 : g.luaactivatemouse = lua_tonumber(L, 1); break;
		case 133 : g.realfov_f = lua_tonumber(L, 1); break;
		case 134 : g.gdisablepeeking = lua_tonumber(L, 1); break;
		case 135 : t.plrhasfocus = lua_tonumber(L, 1); break;
		case 136 : t.game.runasmultiplayer = lua_tonumber(L, 1); break;
		case 137 : g.mp.respawnLeft = lua_tonumber(L, 1); break;
		case 138 : g.tabmode = lua_tonumber(L, 1); break;
		case 139 : g.lowfpswarning = lua_tonumber(L, 1); break;
		case 140 : t.visuals.CameraFOV_f = lua_tonumber(L, 1); break;
		case 141 : t.visuals.CameraFOVZoomed_f = lua_tonumber(L, 1); break;
		case 142 : g.gminvert = lua_tonumber(L, 1); break;
		case 143 : t.plrkeySLOWMOTION = lua_tonumber(L, 1); break;
		case 144 : g.globals.smoothcamerakeys = lua_tonumber(L, 1); break;
		case 145 : t.cammousemovex_f = lua_tonumber(L, 1); break;
		case 146 : t.cammousemovey_f = lua_tonumber(L, 1); break;
		case 147 : g.gunRecoilX_f = lua_tonumber(L, 1); break;
		case 148 : g.gunRecoilY_f = lua_tonumber(L, 1); break;
		case 149 : g.gunRecoilAngleX_f = lua_tonumber(L, 1); break;
		case 150 : g.gunRecoilAngleY_f = lua_tonumber(L, 1); break;
		case 151 : t.gunRecoilCorrectY_f = lua_tonumber(L, 1); break;
		case 152 : g.gunRecoilCorrectX_f = lua_tonumber(L, 1); break;
		case 153 : g.gunRecoilCorrectAngleY_f = lua_tonumber(L, 1); break;
		case 154 : t.gunRecoilCorrectAngleX_f = lua_tonumber(L, 1); break;
		case 155 : t.camangx_f = lua_tonumber(L, 1); break;
		case 156 : t.camangy_f = lua_tonumber(L, 1); break;
		case 157 : t.aisystem.playerducking = lua_tonumber(L, 1); break;
		case 158 : t.conkit.editmodeactive = lua_tonumber(L, 1); break;
		case 159 : t.plrkeySHIFT = lua_tonumber(L, 1); break;
		case 160 : t.plrkeySHIFT2 = lua_tonumber(L, 1); break;
		case 161 : t.inputsys.keycontrol = lua_tonumber(L, 1); break;
		case 162 : t.hardwareinfoglobals.nowater = lua_tonumber(L, 1); break;
		case 163 : t.terrain.waterliney_f = lua_tonumber(L, 1); break;
		case 164 : g.flashLightKeyEnabled = lua_tonumber(L, 1); break;
		case 165 : t.playerlight.flashlightcontrol_f = lua_tonumber(L, 1); break;
		case 166 : t.player[t.plrid].state.moving = lua_tonumber(L, 1); break;
		case 167 : t.tplayerterrainheight_f = lua_tonumber(L, 1); break;
		case 168 : 
			t.tjetpackverticalmove_f = lua_tonumber(L, 1);
			break;
		case 169 : t.terrain.TerrainID = lua_tonumber(L, 1); break;
		case 170 : g.globals.enableplrspeedmods = lua_tonumber(L, 1); break;
		case 171 : g.globals.riftmode = lua_tonumber(L, 1); break;
		case 172 : t.tplayerx_f = lua_tonumber(L, 1); break;
		case 173 : t.tplayery_f = lua_tonumber(L, 1); break;
		case 174 : t.tplayerz_f = lua_tonumber(L, 1); break;
		case 175 : t.terrain.playerx_f = lua_tonumber(L, 1); break;
		case 176 : t.terrain.playery_f = lua_tonumber(L, 1); break;
		case 177 : t.terrain.playerz_f = lua_tonumber(L, 1); break;
		case 178 : t.terrain.playerax_f = lua_tonumber(L, 1); break;
		case 179 : t.terrain.playeray_f = lua_tonumber(L, 1); break;
		case 180 : t.terrain.playeraz_f = lua_tonumber(L, 1); break;
		case 181 : t.tadjustbasedonwobbley_f = lua_tonumber(L, 1); break;
		case 182 : t.tFinalCamX_f = lua_tonumber(L, 1); break;
		case 183 : t.tFinalCamY_f = lua_tonumber(L, 1); break;
		case 184 : t.tFinalCamZ_f = lua_tonumber(L, 1); break;
		case 185 : t.tshakex_f = lua_tonumber(L, 1); break;
		case 186 : t.tshakey_f = lua_tonumber(L, 1); break;
		case 187 : t.tshakez_f = lua_tonumber(L, 1); break;		
		case 188 : t.huddamage.immunity = lua_tonumber(L, 1); break;		
		case 189 : g.charanimindex = lua_tonumber(L, 1); break;	

		// 190-200 reserved for MOTION CONTROLLER actions
		case 201: t.gun[gunId].settings.ismelee = lua_tonumber(L, param); break;
		case 202 : t.gun[gunId].settings.alternate       = lua_tonumber( L, param ); break;
		case 203 : t.gun[gunId].settings.modessharemags  = lua_tonumber( L, param ); break;
		case 204 : t.gun[gunId].settings.alternateisflak = lua_tonumber( L, param ); break;
		case 205 : t.gun[gunId].settings.alternateisray  = lua_tonumber( L, param ); break;
		case 301 : g.firemodes[gunId][fireMode].settings.reloadqty         = lua_tonumber( L, param ); break;
		case 302 : g.firemodes[gunId][fireMode].settings.isempty           = lua_tonumber( L, param ); break;
		case 303 : g.firemodes[gunId][fireMode].settings.jammed            = lua_tonumber( L, param ); break;
		case 304 : g.firemodes[gunId][fireMode].settings.jamchance         = lua_tonumber( L, param ); break;
		case 305 : g.firemodes[gunId][fireMode].settings.mintimer          = lua_tonumber( L, param ); break;
		case 306 : g.firemodes[gunId][fireMode].settings.addtimer          = lua_tonumber( L, param ); break;
		case 307 : g.firemodes[gunId][fireMode].settings.shotsfired        = lua_tonumber( L, param ); break;
		case 308 : g.firemodes[gunId][fireMode].settings.cooltimer         = lua_tonumber( L, param ); break;
		case 309 : g.firemodes[gunId][fireMode].settings.overheatafter     = lua_tonumber( L, param ); break;
		case 310 : g.firemodes[gunId][fireMode].settings.jamchancetime     = lua_tonumber( L, param ); break;
		case 311 : g.firemodes[gunId][fireMode].settings.cooldown          = lua_tonumber( L, param ); break;
		case 312 : g.firemodes[gunId][fireMode].settings.nosubmergedfire   = lua_tonumber( L, param ); break;
		case 313 : g.firemodes[gunId][fireMode].settings.simplezoom        = lua_tonumber( L, param ); break;
		case 314 : g.firemodes[gunId][fireMode].settings.forcezoomout      = lua_tonumber( L, param ); break;
		case 315 : g.firemodes[gunId][fireMode].settings.zoommode          = lua_tonumber( L, param ); break;
		case 316 : g.firemodes[gunId][fireMode].settings.simplezoomanim    = lua_tonumber( L, param ); break;
		case 317 : g.firemodes[gunId][fireMode].settings.poolindex         = lua_tonumber( L, param ); break;
		case 318 : g.firemodes[gunId][fireMode].settings.plrturnspeedmod   = lua_tonumber( L, param ); break;
		case 319 : g.firemodes[gunId][fireMode].settings.zoomturnspeed     = lua_tonumber( L, param ); break;
		case 320 : g.firemodes[gunId][fireMode].settings.plrjumpspeedmod   = lua_tonumber( L, param ); break;
		case 321 : g.firemodes[gunId][fireMode].settings.plremptyspeedmod  = lua_tonumber( L, param ); break;
		case 322 : g.firemodes[gunId][fireMode].settings.plrmovespeedmod   = lua_tonumber( L, param ); break;
		case 323 : g.firemodes[gunId][fireMode].settings.zoomwalkspeed     = lua_tonumber( L, param ); break;
		case 324 : g.firemodes[gunId][fireMode].settings.plrreloadspeedmod = lua_tonumber( L, param ); break;
		case 325 : g.firemodes[gunId][fireMode].settings.hasempty          = lua_tonumber( L, param ); break;
		case 326 : g.firemodes[gunId][fireMode].action.block.s             = lua_tonumber( L, param ); break;

		// for extra commands as yet unimagined :)
		case 401: t.playerlight.flashlightcontrol_range_f = lua_tonumber(L, 1);		break;
		case 402: t.playerlight.flashlightcontrol_radius_f = lua_tonumber(L, 1);	break;
		case 403: t.playerlight.flashlightcontrol_colorR_f = lua_tonumber(L, 1);	break;
		case 404: t.playerlight.flashlightcontrol_colorG_f = lua_tonumber(L, 1);	break;
		case 405: t.playerlight.flashlightcontrol_colorB_f = lua_tonumber(L, 1);	break;
		case 406: t.playerlight.flashlightcontrol_cashshadow = lua_tonumber(L, 1);	break;

		case 501 : t.gunsound[t.gunid][lua_tonumber(L, 1)].soundid1 = lua_tonumber(L, 2); break;
		case 502 : t.gunsound[t.gunid][lua_tonumber(L, 1)].altsoundid = lua_tonumber(L, 2); break;
		case 503 : break;
		case 504 : break;

		case 601: t.player[t.plrid].state.counteredaction = lua_tonumber(L, 1); break;

		case 700 :  iSrc = lua_tonumber(L, 1);
					iDest = lua_tonumber(L, 2);
					if ( iDest == 0 ) 
						t.charanimstate = t.charanimstates[lua_tonumber(L, 1)]; 
					else
						if ( iSrc == 0 ) 
							t.charanimstates[iDest] = t.charanimstate;
						else
							t.charanimstates[iDest] = t.charanimstates[iSrc]; 
					break;
		case 701 :	iDest = lua_tonumber(L, 1);
					if ( iDest == 0 ) 
						t.charanimstate.playcsi = lua_tonumber(L, 2); 
					else
						t.charanimstates[iDest].playcsi = lua_tonumber(L, 2); 
					break;
		case 702 :	iDest = lua_tonumber(L, 1);
					if ( iDest == 0 ) 
						t.charanimstate.originale = lua_tonumber(L, 2); 
					else
						t.charanimstates[iDest].originale = lua_tonumber(L, 2); 
					break;
		case 703 :	iDest = lua_tonumber(L, 1);
					if ( iDest == 0 ) 
						t.charanimstate.obj = lua_tonumber(L, 2); 
					else
						t.charanimstates[iDest].obj = lua_tonumber(L, 2); 
					break;
		case 704 :	iDest = lua_tonumber(L, 1);
					if ( iDest == 0 ) 
						t.charanimstate.animationspeed_f = lua_tonumber(L, 2); 
					else
						t.charanimstates[iDest].animationspeed_f = lua_tonumber(L, 2); 
					break;
		case 705 :	iDest = lua_tonumber(L, 1);
					if ( iDest == 0 ) 
						t.charanimstate.e = lua_tonumber(L, 2); 
					else
						t.charanimstates[iDest].e = lua_tonumber(L, 2); 
					break;

		case 801 : t.charanimcontrols[lua_tonumber(L, 1)].leaping = lua_tonumber(L, 2); 
			break;
		case 802 : t.charanimcontrols[lua_tonumber(L, 1)].moving = lua_tonumber(L, 2); 
			break;

		case 1001 : break;
		case 1002 : break;
	}
	return 0;
}
int GetGamePlayerControlData ( lua_State *L, int iDataMode )
{
	lua = L;
	int iSrc = 0;
	int n = LUA_GETTOP(L);
	if ( iDataMode >= 500 )
	{
		if ( iDataMode >= 1001 )
			if ( n < 2 ) return 0;
		else
			if ( n < 1 ) return 0;
	}
	int gunId = t.gunid;
	int fireMode = t.tfiremode;

	if ( n > 0 && iDataMode > 200 && iDataMode < 500 )
	{
		gunId = lua_tonumber(L, 1);
		if (n > 1)
		{
			fireMode = lua_tonumber(L, 2);
		}
		else
		{
			fireMode = 0;
		}
	}

	switch ( iDataMode )
	{
		case 1 : lua_pushnumber ( L, t.playercontrol.jetpackmode ); break;
		case 2 : lua_pushnumber ( L, t.playercontrol.jetpackfuel_f ); break;
		case 3 : lua_pushnumber ( L, t.playercontrol.jetpackhidden ); break;
		case 4 : lua_pushnumber ( L, t.playercontrol.jetpackcollected ); break;
		case 5 : lua_pushnumber ( L, t.playercontrol.soundstartindex ); break;
		case 6 : lua_pushnumber ( L, t.playercontrol.jetpackparticleemitterindex ); break;
		case 7 : lua_pushnumber ( L, t.playercontrol.jetpackthrust_f ); break;
		case 8 : lua_pushnumber ( L, t.playercontrol.startstrength ); break;
		case 9 : lua_pushnumber ( L, t.playercontrol.isrunning ); break;
		case 10 : break;
		case 11 : lua_pushnumber ( L, t.playercontrol.cx_f ); break;
		case 12 : lua_pushnumber ( L, t.playercontrol.cy_f ); break;
		case 13 : lua_pushnumber ( L, t.playercontrol.cz_f ); break;
		case 14 : lua_pushnumber ( L, t.playercontrol.basespeed_f ); break;
		case 15 : lua_pushnumber ( L, t.playercontrol.canrun ); break;
		case 16 : lua_pushnumber ( L, t.playercontrol.maxspeed_f ); break;
		case 17 : lua_pushnumber ( L, t.playercontrol.topspeed_f ); break;
		case 18 : lua_pushnumber ( L, t.playercontrol.movement ); break;
		case 19 : lua_pushnumber ( L, t.playercontrol.movey_f ); break;
		case 20 : lua_pushnumber ( L, t.playercontrol.lastmovement ); break;
		case 21 : lua_pushnumber ( L, t.playercontrol.footfallcount ); break;
		case 22 : break;
		case 23 : lua_pushnumber ( L, t.playercontrol.gravityactive ); break;
		case 24 : lua_pushnumber ( L, t.playercontrol.plrhitfloormaterial ); break;
		case 25:  lua_pushnumber(L, t.playercontrol.underwater); break;
		case 26 : lua_pushnumber ( L, t.playercontrol.jumpmode ); break;
		case 27 : lua_pushnumber ( L, t.playercontrol.jumpmodecanaffectvelocitycountdown_f ); break;
		case 28 : lua_pushnumber ( L, t.playercontrol.speed_f ); break;
		case 29 : lua_pushnumber ( L, t.playercontrol.accel_f ); break;
		case 30 : lua_pushnumber ( L, t.playercontrol.speedratio_f ); break;
		case 31 : lua_pushnumber ( L, t.playercontrol.wobble_f ); break;
		case 32 :
		{
#ifdef FASTBULLETPHYSICS
			lua_pushnumber(L, t.playercontrol.wobblespeed_f*0.5);
#else
			extern bool bPhysicsRunningAt120FPS;
			if (!bPhysicsRunningAt120FPS)
				lua_pushnumber(L, t.playercontrol.wobblespeed_f*0.5);
			else
				lua_pushnumber(L, t.playercontrol.wobblespeed_f);
#endif
			break;
		}
		case 33 : lua_pushnumber ( L, t.playercontrol.wobbleheight_f ); break;
		case 34 : lua_pushnumber ( L, t.playercontrol.jumpmax_f ); break;
		case 35 : lua_pushnumber ( L, t.playercontrol.pushangle_f ); break;
		case 36 : lua_pushnumber ( L, t.playercontrol.pushforce_f ); break;
		case 37 : lua_pushnumber ( L, t.playercontrol.footfallpace_f ); break;
		case 38 : lua_pushnumber ( L, t.playercontrol.lockatheight ); break;
		case 39 : lua_pushnumber ( L, t.playercontrol.controlheight ); break;
		case 40 : lua_pushnumber ( L, t.playercontrol.controlheightcooldown ); break;
		case 41 : lua_pushnumber ( L, t.playercontrol.storemovey ); break;
		case 42 : break;
		case 43 : lua_pushnumber ( L, t.playercontrol.hurtfall ); break;
		case 44 : lua_pushnumber ( L, t.playercontrol.leanoverangle_f ); break;
		case 45 : lua_pushnumber ( L, t.playercontrol.leanover_f ); break;
		case 46 : lua_pushnumber ( L, t.playercontrol.camerashake_f ); break;
		case 47 : lua_pushnumber ( L, t.playercontrol.finalcameraanglex_f ); break;
		case 48 : lua_pushnumber ( L, t.playercontrol.finalcameraangley_f ); break;
		case 49 : lua_pushnumber ( L, t.playercontrol.finalcameraanglez_f ); break;
		case 50 : lua_pushnumber ( L, t.playercontrol.camrightmousemode ); break;
		case 51 : lua_pushnumber ( L, t.playercontrol.camcollisionsmooth ); break;
		case 52 : lua_pushnumber ( L, t.playercontrol.camcurrentdistance ); break;
		case 53 : lua_pushnumber ( L, t.playercontrol.camdofullraycheck ); break;
		case 54 : lua_pushnumber ( L, t.playercontrol.lastgoodcx_f ); break;
		case 55 : lua_pushnumber ( L, t.playercontrol.lastgoodcy_f ); break;
		case 56 : lua_pushnumber ( L, t.playercontrol.lastgoodcz_f ); break;
		case 57 : break;
		case 58 : lua_pushnumber ( L, t.playercontrol.flinchx_f ); break;
		case 59 : lua_pushnumber ( L, t.playercontrol.flinchy_f ); break;
		case 60 : lua_pushnumber ( L, t.playercontrol.flinchz_f ); break;
		case 61 : lua_pushnumber ( L, t.playercontrol.flinchcurrentx_f ); break;
		case 62 : lua_pushnumber ( L, t.playercontrol.flinchcurrenty_f ); break;
		case 63 : lua_pushnumber ( L, t.playercontrol.flinchcurrentz_f ); break;
		case 64 : lua_pushnumber ( L, t.playercontrol.footfalltype ); break;
		case 65 : lua_pushnumber ( L, t.playercontrol.ripplecount_f ); break;
		case 66 : lua_pushnumber ( L, t.playercontrol.lastfootfallsound ); break;
		case 67 : lua_pushnumber ( L, t.playercontrol.inwaterstate ); break;
		case 68 : lua_pushnumber ( L, t.playercontrol.drowntimestamp ); break;
		case 69 : lua_pushnumber ( L, t.playercontrol.deadtime ); break;
		case 70 : lua_pushnumber ( L, t.playercontrol.swimtimestamp ); break;
		case 71 : lua_pushnumber ( L, t.playercontrol.redDeathFog_f ); break;
		case 72 : 
			#ifdef WICKEDENGINE
			if (t.playercontrol.iPlayHeartBeatSoundOff == 1)
				lua_pushnumber (L, -1);
			else
				lua_pushnumber (L, t.playercontrol.heartbeatTimeStamp);
			#else
			lua_pushnumber (L, t.playercontrol.heartbeatTimeStamp);
			#endif
			break;
		case 81 : lua_pushnumber ( L, t.playercontrol.thirdperson.enabled ); break;
		case 82 : lua_pushnumber ( L, t.playercontrol.thirdperson.characterindex ); break;
		case 83 : lua_pushnumber ( L, t.playercontrol.thirdperson.camerafollow ); break;
		case 84 : lua_pushnumber ( L, t.playercontrol.thirdperson.camerafocus ); break;
		case 85 : lua_pushnumber ( L, t.playercontrol.thirdperson.charactere ); break;
		case 86 : break;
		case 87 : lua_pushnumber ( L, t.playercontrol.thirdperson.shotfired ); break;
		case 88 : lua_pushnumber ( L, t.playercontrol.thirdperson.cameradistance ); break;
		case 89 : lua_pushnumber ( L, t.playercontrol.thirdperson.cameraspeed ); break;
		case 90 : lua_pushnumber ( L, t.playercontrol.thirdperson.cameralocked ); break;
		case 91 : lua_pushnumber ( L, t.playercontrol.thirdperson.cameraheight ); break;
		case 92 : lua_pushnumber ( L, t.playercontrol.thirdperson.camerashoulder ); break;
		
		case 95 : lua_pushnumber(L, t.playercontrol.fFallDamageMultiplier); break;
		case 96: lua_pushnumber(L, t.playercontrol.fSwimSpeed); break;

		case 99 : lua_pushnumber ( L, g.gxboxcontrollertype ); break;		
		case 101 : lua_pushnumber ( L, t.gunmode ); break;
		case 102 : lua_pushnumber ( L, t.player[t.plrid].state.firingmode ); break;
		case 103 : lua_pushnumber ( L, g.weaponammoindex ); break;
		case 104 : lua_pushnumber ( L, g.ammooffset ); break;
		case 105 : lua_pushnumber ( L, g.ggunmeleekey ); break;
		case 106 : lua_pushnumber ( L, t.player[t.plrid].state.blockingaction ); break;
		case 107 : lua_pushnumber ( L, t.gunshootnoammo ); break;		

		case 108 :
			//PE: Now controlled in lua. old (lua_pushnumber(L, g.playerunderwater); )
			if(t.playercontrol.inwaterstate >= 2)
				lua_pushnumber ( L, 1 );
			else
				lua_pushnumber(L, 0);
			break;

		case 109 : lua_pushnumber ( L, g.gdisablerightmousehold ); break;		
		case 110 : lua_pushnumber ( L, g.gxbox ); break;		
		case 111 : lua_pushnumber ( L, JoystickX() ); break;
		case 112 : lua_pushnumber ( L, JoystickY() ); break;
		case 113 : lua_pushnumber ( L, JoystickZ() ); break;
		case 114 : lua_pushnumber ( L, t.gunzoommode ); break;		
		case 115 : lua_pushnumber ( L, t.gunzoommag_f ); break;		
		case 116 : lua_pushnumber ( L, t.gunreloadnoammo ); break;	
		case 117 : lua_pushnumber ( L, g.plrreloading ); break;	
		case 118 : lua_pushnumber ( L, g.ggunaltswapkey1 ); break;	
		case 119 : lua_pushnumber ( L, g.ggunaltswapkey2 ); break;	
		case 120 : lua_pushnumber ( L, t.weaponkeyselection ); break;	
		case 121 : lua_pushnumber ( L, t.weaponindex ); break;	
		case 122 : lua_pushnumber ( L, t.player[t.plrid].command.newweapon ); break;	
		case 123 : lua_pushnumber ( L, t.gunid ); break;	
		case 124 : lua_pushnumber ( L, t.gunselectionafterhide ); break;	
		case 125 : lua_pushnumber ( L, t.gunburst ); break;	
		case 126 : break;
		case 127 : lua_pushnumber ( L, JoystickTwistX() ); break;
		case 128 : lua_pushnumber ( L, JoystickTwistY() ); break;
		case 129 : lua_pushnumber ( L, JoystickTwistZ() ); break;
		case 130 : lua_pushnumber ( L, t.plrzoominchange ); break;	
		case 131 : lua_pushnumber ( L, t.plrzoomin_f ); break;	
		case 132 : lua_pushnumber ( L, g.luaactivatemouse ); break;	
		case 133 : lua_pushnumber ( L, g.realfov_f ); break;	
		case 134 : lua_pushnumber ( L, g.gdisablepeeking ); break;	
		case 135 : lua_pushnumber ( L, t.plrhasfocus ); break;	
		case 136 : lua_pushnumber ( L, t.game.runasmultiplayer ); break;	
		case 137 : lua_pushnumber ( L, g.mp.respawnLeft ); break;	
		case 138 : lua_pushnumber ( L, g.tabmode ); break;	
		case 139 : lua_pushnumber ( L, g.lowfpswarning ); break;	
		case 140 : lua_pushnumber ( L, t.visuals.CameraFOV_f ); break;	
		case 141 : lua_pushnumber ( L, t.visuals.CameraFOVZoomed_f ); break;	
		case 142 : lua_pushnumber ( L, g.gminvert ); break;	
		case 143 : lua_pushnumber ( L, t.plrkeySLOWMOTION ); break;	
		case 144 : lua_pushnumber ( L, g.globals.smoothcamerakeys ); break;	
		case 145 : lua_pushnumber ( L, t.cammousemovex_f ); break;	
		case 146 : lua_pushnumber ( L, t.cammousemovey_f ); break;	
		case 147 : lua_pushnumber ( L, g.gunRecoilX_f ); break;	
		case 148 : lua_pushnumber ( L, g.gunRecoilY_f ); break;	
		case 149 : lua_pushnumber ( L, g.gunRecoilAngleX_f ); break;	
		case 150 : lua_pushnumber ( L, g.gunRecoilAngleY_f ); break;	
		case 151 : lua_pushnumber ( L, t.gunRecoilCorrectY_f ); break;	
		case 152 : lua_pushnumber ( L, g.gunRecoilCorrectX_f ); break;	
		case 153 : lua_pushnumber ( L, g.gunRecoilCorrectAngleY_f ); break;	
		case 154 : lua_pushnumber ( L, t.gunRecoilCorrectAngleX_f ); break;	
		case 155 : lua_pushnumber ( L, t.camangx_f ); break;	
		case 156 : lua_pushnumber ( L, t.camangy_f ); break;	
		case 157 : lua_pushnumber ( L, t.aisystem.playerducking ); break;	
		case 158 : lua_pushnumber ( L, t.conkit.editmodeactive ); break;	
		case 159 : lua_pushnumber ( L, t.plrkeySHIFT ); break;	
		case 160 : lua_pushnumber ( L, t.plrkeySHIFT2 ); break;	
		case 161 : lua_pushnumber ( L, t.inputsys.keycontrol ); break;	
		case 162 : lua_pushnumber ( L, t.hardwareinfoglobals.nowater ); break;	
		case 163 : lua_pushnumber ( L, t.terrain.waterliney_f ); break;	
		case 164 : lua_pushnumber ( L, g.flashLightKeyEnabled ); break;	
		case 165 : lua_pushnumber ( L, t.playerlight.flashlightcontrol_f ); break;	
		case 166 : lua_pushnumber ( L, t.player[t.plrid].state.moving ); break;	
		case 167 : lua_pushnumber ( L, t.tplayerterrainheight_f ); break;	
		case 168 : 
			lua_pushnumber ( L, t.tjetpackverticalmove_f );
			break;	
		case 169 : lua_pushnumber ( L, t.terrain.TerrainID ); break;	
		case 170 : lua_pushnumber ( L, g.globals.enableplrspeedmods ); break;	
		case 171 : lua_pushnumber ( L, g.globals.riftmode ); break;	
		case 172 : lua_pushnumber ( L, t.tplayerx_f ); break;	
		case 173 : lua_pushnumber ( L, t.tplayery_f ); break;	
		case 174 : lua_pushnumber ( L, t.tplayerz_f ); break;	
		case 175 : lua_pushnumber ( L, t.terrain.playerx_f ); break;	
		case 176 : lua_pushnumber ( L, t.terrain.playery_f ); break;	
		case 177 : lua_pushnumber ( L, t.terrain.playerz_f ); break;	
		case 178 : lua_pushnumber ( L, t.terrain.playerax_f ); break;	
		case 179 : lua_pushnumber ( L, t.terrain.playeray_f ); break;	
		case 180 : lua_pushnumber ( L, t.terrain.playeraz_f ); break;	
		case 181 : lua_pushnumber ( L, t.tadjustbasedonwobbley_f ); break;	
		case 182 : lua_pushnumber ( L, t.tFinalCamX_f ); break;	
		case 183 : lua_pushnumber ( L, t.tFinalCamY_f ); break;	
		case 184 : lua_pushnumber ( L, t.tFinalCamZ_f ); break;	
		case 185 : lua_pushnumber ( L, t.tshakex_f ); break;	
		case 186 : lua_pushnumber ( L, t.tshakey_f); break;	
		case 187 : lua_pushnumber ( L, t.tshakez_f ); break;	
		case 188 : lua_pushnumber ( L, t.huddamage.immunity ); break;	
		case 189 : lua_pushnumber ( L, g.charanimindex ); break;				

		#ifdef VRTECH
		case 190 : if ( GGVR_IsHmdPresent() > 0 ) { lua_pushnumber ( L, 1 ); } else { lua_pushnumber ( L, 0 ); } break;
		case 191 : lua_pushnumber ( L, GGVR_IsHmdPresent() ); break;				
		case 192 : lua_pushnumber ( L, GGVR_BestController_JoyX() ); break;
		case 193 : lua_pushnumber ( L, GGVR_BestController_JoyY() ); break;
		case 194 : lua_pushnumber ( L, GGVR_RightController_Trigger() ); break;
		case 195 : lua_pushnumber ( L, GGVR_RightController_Grip() ); break;
		case 196 : lua_pushnumber ( L, GGVR_RightController_JoyX() ); break;
		case 197: lua_pushnumber (L, GGVR_RightController_JoyY()); break;
		case 198: lua_pushnumber (L, GGVR_RightController_Button1()); break;
		case 199: lua_pushnumber (L, GGVR_RightController_Button2()); break;
		case 251 : lua_pushnumber ( L, GGVR_GetBestHandX() ); break;
		case 252 : lua_pushnumber ( L, GGVR_GetBestHandY() ); break;
		case 253 : lua_pushnumber ( L, GGVR_GetBestHandZ() ); break;
		case 254 : lua_pushnumber ( L, GGVR_GetBestHandAngleX() ); break;
		case 255 : lua_pushnumber ( L, GGVR_GetBestHandAngleY() ); break;
		case 256 : lua_pushnumber ( L, GGVR_GetBestHandAngleZ() ); break;
		case 257: lua_pushnumber (L, GGVR_GetLaserGuidedEntityObj(g.entityviewstartobj, g.entityviewendobj)); break;
		#else
		case 190 : 
		case 191 : 
		case 192 : 
		case 193 : 
		case 194 : 
		case 195 : 
		case 196 : 
		case 197 : 
		case 198 : 
		case 199 : 
		case 200 : 
		case 251 : 
		case 252 : 
		case 253 : 
		case 254 : 
		case 255 : 
		case 256 : 
		case 257 : 
			lua_pushnumber ( L, 0 ); 
			break;
		#endif

		case 201: lua_pushnumber (L, t.gun[gunId].settings.ismelee); break;
		case 202 : lua_pushnumber ( L, t.gun[gunId].settings.alternate       ); break;
		case 203 : lua_pushnumber ( L, t.gun[gunId].settings.modessharemags  ); break;
		case 204 : lua_pushnumber ( L, t.gun[gunId].settings.alternateisflak ); break;
		case 205 : lua_pushnumber ( L, t.gun[gunId].settings.alternateisray  ); break;
		
		// 251-260 used above

		case 301 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.reloadqty         ); break;
		case 302 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.isempty           ); break;
		case 303 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.jammed            ); break;
		case 304 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.jamchance         ); break;
		case 305 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.mintimer          ); break;
		case 306 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.addtimer          ); break;
		case 307 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.shotsfired        ); break;
		case 308 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.cooltimer         ); break;
		case 309 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.overheatafter     ); break;
		case 310 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.jamchancetime     ); break;
		case 311 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.cooldown          ); break;
		case 312 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.nosubmergedfire   ); break;
		case 313 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.simplezoom        ); break;
		case 314 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.forcezoomout      ); break;
		case 315 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.zoommode          ); break;
		case 316 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.simplezoomanim    ); break;
		case 317 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.poolindex         ); break;
		case 318 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.plrturnspeedmod   ); break;
		case 319 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.zoomturnspeed     ); break;
		case 320 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.plrjumpspeedmod   ); break;
		case 321 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.plremptyspeedmod  ); break;
		case 322 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.plrmovespeedmod   ); break;
		case 323 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.zoomwalkspeed     ); break;
		case 324 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.plrreloadspeedmod ); break;
		case 325 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.hasempty          ); break;
		case 326 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].action.block.s			   ); break;
		case 327 : lua_pushnumber ( L, g.firemodes[gunId][fireMode].settings.meleewithrightclick ); break;
		case 328: lua_pushnumber (L, g.firemodes[gunId][fireMode].settings.blockwithrightclick); break;

		// for extra commands as yet unimagined :)
		case 401: lua_pushnumber (L, t.playerlight.flashlightcontrol_range_f); break;
		case 402: lua_pushnumber (L, t.playerlight.flashlightcontrol_radius_f); break;
		case 403: lua_pushnumber (L, t.playerlight.flashlightcontrol_colorR_f); break;
		case 404: lua_pushnumber (L, t.playerlight.flashlightcontrol_colorG_f); break;
		case 405: lua_pushnumber (L, t.playerlight.flashlightcontrol_colorB_f); break;
		case 406: lua_pushnumber (L, t.playerlight.flashlightcontrol_cashshadow); break;

		case 501 : lua_pushnumber ( L, t.gunsound[t.gunid][lua_tonumber(L, 1)].soundid1 ); break;
		case 502 : lua_pushnumber ( L, t.gunsound[t.gunid][lua_tonumber(L, 1)].altsoundid ); break;		
		case 503 : lua_pushnumber ( L, JoystickHatAngle(lua_tonumber(L, 1)) ); break;
		case 504 : lua_pushnumber ( L, JoystickFireXL(lua_tonumber(L, 1)) ); break;

		case 601: lua_pushnumber (L, t.player[t.plrid].state.counteredaction); break;
			
		case 701 :	iSrc = lua_tonumber(L, 1);
					if ( iSrc == 0 )
						lua_pushnumber ( L, t.charanimstate.playcsi ); 
					else
						lua_pushnumber ( L, t.charanimstates[iSrc].playcsi ); 
					break;
		case 702 :	iSrc = lua_tonumber(L, 1);
					if ( iSrc == 0 )
						lua_pushnumber ( L, t.charanimstate.originale ); 
					else
						lua_pushnumber ( L, t.charanimstates[iSrc].originale ); 
					break;
		case 703 :	iSrc = lua_tonumber(L, 1);
					if ( iSrc == 0 )
						lua_pushnumber ( L, t.charanimstate.obj ); 
					else
						lua_pushnumber ( L, t.charanimstates[iSrc].obj ); 
					break;
		case 704 :	iSrc = lua_tonumber(L, 1);
					if ( iSrc == 0 )
						lua_pushnumber ( L, t.charanimstate.animationspeed_f ); 
					else
						lua_pushnumber ( L, t.charanimstates[iSrc].animationspeed_f ); 
					break;
		case 705 :	iSrc = lua_tonumber(L, 1);
					if ( iSrc == 0 )
						lua_pushnumber ( L, t.charanimstate.e ); 
					else
						lua_pushnumber ( L, t.charanimstates[iSrc].e ); 
					break;

		case 741 : lua_pushnumber ( L, t.csi_stoodvault[lua_tonumber(L, 1)] ); break;
		case 751 : lua_pushnumber ( L, t.charseq[lua_tonumber(L, 1)].trigger ); break;
		case 761 : lua_pushnumber ( L, LuaEntityIDValid ( L, lua_tonumber(L, 1), 1 ) ? t.entityelement[lua_tonumber(L, 1)].bankindex : 0 ); break;
		case 762 : lua_pushnumber ( L, LuaEntityIDValid ( L, lua_tonumber(L, 1), 1 ) ? t.entityelement[lua_tonumber(L, 1)].obj : 0 ); break;
		case 763 : lua_pushnumber ( L, LuaEntityIDValid ( L, lua_tonumber(L, 1), 1 ) ? t.entityelement[lua_tonumber(L, 1)].ragdollified : 0 ); break;
		case 764 : lua_pushnumber ( L, LuaEntityIDValid ( L, lua_tonumber(L, 1), 1 ) ? t.entityelement[lua_tonumber(L, 1)].speedmodulator_f : 0 ); break;
		case 801 : lua_pushnumber ( L, t.charanimcontrols[lua_tonumber(L, 1)].leaping ); break;
		case 802 : lua_pushnumber ( L, t.charanimcontrols[lua_tonumber(L, 1)].moving ); break;
		case 851 : lua_pushnumber ( L, t.entityprofile[lua_tonumber(L, 1)].fJumpModifier ); break;
		case 852 : lua_pushnumber ( L, t.entityprofile[lua_tonumber(L, 1)].startofaianim ); break;			
		case 853 : lua_pushnumber ( L, t.entityprofile[lua_tonumber(L, 1)].jumphold ); break;			
		case 854 : lua_pushnumber ( L, t.entityprofile[lua_tonumber(L, 1)].jumpresume ); break;			

		case 1001 : lua_pushnumber ( L, t.entityanim[lua_tonumber(L, 1)][lua_tonumber(L, 2)].start ); break;
		case 1002 : lua_pushnumber ( L, t.entityanim[lua_tonumber(L, 1)][lua_tonumber(L, 2)].finish ); break;
	}
	return 1;
}
int GetPlayerFov (lua_State* L)
{
	// to match the SetPlayerFOV command - or you could have used 'g_PlayerFOV'
	int iPlayerFOVPerc = (((t.visuals.CameraFOV_f * t.visuals.CameraASPECT_f) - 20.0) / 180.0) * 114.0f;// 100.0;
	lua_pushnumber (L, iPlayerFOVPerc);
	return 1;
}
int GetPlayerAttacking (lua_State* L)
{
	int iPlayerIsAttackingNow = 0;
	if (t.gunmode >= 101 && t.gunmode < 110) iPlayerIsAttackingNow = 1;
	if(t.gunmode >= 1020 && t.gunmode < 1029 ) iPlayerIsAttackingNow = 1;
	lua_pushnumber (L, iPlayerIsAttackingNow);
	return 1;
}
int PushPlayer (lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	// trigger short or long player arms animation (jerked back; typically when counter attacked)
	t.gunmode = 1011;
	return 0;
}

int GetGamePlayerControlJetpackMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 1 ); }
int GetGamePlayerControlJetpackFuel ( lua_State *L ) { return GetGamePlayerControlData ( L, 2 ); }
int GetGamePlayerControlJetpackHidden ( lua_State *L ) { return GetGamePlayerControlData ( L, 3 ); }
int GetGamePlayerControlJetpackCollected ( lua_State *L ) { return GetGamePlayerControlData ( L, 4 ); }
int GetGamePlayerControlSoundStartIndex ( lua_State *L ) { return GetGamePlayerControlData ( L, 5 ); }
int GetGamePlayerControlJetpackParticleEmitterIndex ( lua_State *L ) { return GetGamePlayerControlData ( L, 6 ); }
int GetGamePlayerControlJetpackThrust ( lua_State *L ) { return GetGamePlayerControlData ( L, 7 ); }
int GetGamePlayerControlStartStrength ( lua_State *L ) { return GetGamePlayerControlData ( L, 8 ); }
int GetGamePlayerControlIsRunning ( lua_State *L ) { return GetGamePlayerControlData ( L, 9 ); }
int GetGamePlayerControlX6 ( lua_State *L ) { return GetGamePlayerControlData ( L, 10 ); }
int GetGamePlayerControlCx ( lua_State *L ) { return GetGamePlayerControlData ( L, 11 ); }
int GetGamePlayerControlCy ( lua_State *L ) { return GetGamePlayerControlData ( L, 12 ); }
int GetGamePlayerControlCz ( lua_State *L ) { return GetGamePlayerControlData ( L, 13 ); }
int GetGamePlayerControlBasespeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 14 ); }
int GetGamePlayerControlCanRun ( lua_State *L ) { return GetGamePlayerControlData ( L, 15 ); }
int GetGamePlayerControlMaxspeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 16 ); }
int GetGamePlayerControlTopspeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 17 ); }
int GetGamePlayerControlMovement ( lua_State *L ) { return GetGamePlayerControlData ( L, 18 ); }
int GetGamePlayerControlMovey ( lua_State *L ) { return GetGamePlayerControlData ( L, 19 ); }
int GetGamePlayerControlLastMovement ( lua_State *L ) { return GetGamePlayerControlData ( L, 20 ); }
int GetGamePlayerControlFootfallCount ( lua_State *L ) { return GetGamePlayerControlData ( L, 21 ); }
int GetGamePlayerControlX1 ( lua_State *L ) { return GetGamePlayerControlData ( L, 22 ); }
int GetGamePlayerControlGravityActive ( lua_State *L ) { return GetGamePlayerControlData ( L, 23 ); }
int GetGamePlayerControlPlrHitFloorMaterial ( lua_State *L ) { return GetGamePlayerControlData ( L, 24 ); }
int GetGamePlayerControlUnderwater ( lua_State *L ) { return GetGamePlayerControlData ( L, 25 ); }
int GetGamePlayerControlJumpMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 26 ); }
int GetGamePlayerControlJumpModeCanAffectVelocityCountdown ( lua_State *L ) { return GetGamePlayerControlData ( L, 27 ); }
int GetGamePlayerControlSpeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 28 ); }
int GetGamePlayerControlAccel ( lua_State *L ) { return GetGamePlayerControlData ( L, 29 ); }
int GetGamePlayerControlSpeedRatio ( lua_State *L ) { return GetGamePlayerControlData ( L, 30 ); }
int GetGamePlayerControlWobble ( lua_State *L ) { return GetGamePlayerControlData ( L, 31 ); }
int GetGamePlayerControlWobbleSpeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 32 ); }
int GetGamePlayerControlWobbleHeight ( lua_State *L ) { return GetGamePlayerControlData ( L, 33 ); }
int GetGamePlayerControlJumpmax ( lua_State *L ) { return GetGamePlayerControlData ( L, 34 ); }
int GetGamePlayerControlPushangle ( lua_State *L ) { return GetGamePlayerControlData ( L, 35 ); }
int GetGamePlayerControlPushforce ( lua_State *L ) { return GetGamePlayerControlData ( L, 36 ); }
int GetGamePlayerControlFootfallPace ( lua_State *L ) { return GetGamePlayerControlData ( L, 37 ); }
int GetGamePlayerControlLockAtHeight ( lua_State *L ) { return GetGamePlayerControlData ( L, 38 ); }
int GetGamePlayerControlControlHeight ( lua_State *L ) { return GetGamePlayerControlData ( L, 39 ); }
int GetGamePlayerControlControlHeightCooldown ( lua_State *L ) { return GetGamePlayerControlData ( L, 40 ); }
int GetGamePlayerControlStoreMovey ( lua_State *L ) { return GetGamePlayerControlData ( L, 41 ); }
int GetGamePlayerControlX3 ( lua_State *L ) { return GetGamePlayerControlData ( L, 42 ); }
int GetGamePlayerControlHurtFall ( lua_State *L ) { return GetGamePlayerControlData ( L, 43 ); }
int GetGamePlayerControlLeanoverAngle ( lua_State *L ) { return GetGamePlayerControlData ( L, 44 ); }
int GetGamePlayerControlLeanover ( lua_State *L ) { return GetGamePlayerControlData ( L, 45 ); }
int GetGamePlayerControlCameraShake ( lua_State *L ) { return GetGamePlayerControlData ( L, 46 ); }
int GetGamePlayerControlFinalCameraAnglex ( lua_State *L ) { return GetGamePlayerControlData ( L, 47 ); }
int GetGamePlayerControlFinalCameraAngley ( lua_State *L ) { return GetGamePlayerControlData ( L, 48 ); }
int GetGamePlayerControlFinalCameraAnglez ( lua_State *L ) { return GetGamePlayerControlData ( L, 49 ); }
int GetGamePlayerControlCamRightMouseMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 50 ); }
int GetGamePlayerControlCamCollisionSmooth ( lua_State *L ) { return GetGamePlayerControlData ( L, 51 ); }
int GetGamePlayerControlCamCurrentDistance ( lua_State *L ) { return GetGamePlayerControlData ( L, 52 ); }
int GetGamePlayerControlCamDoFullRayCheck ( lua_State *L ) { return GetGamePlayerControlData ( L, 53 ); }
int GetGamePlayerControlLastGoodcx ( lua_State *L ) { return GetGamePlayerControlData ( L, 54 ); }
int GetGamePlayerControlLastGoodcy ( lua_State *L ) { return GetGamePlayerControlData ( L, 55 ); }
int GetGamePlayerControlLastGoodcz ( lua_State *L ) { return GetGamePlayerControlData ( L, 56 ); }
int GetGamePlayerControlX4 ( lua_State *L ) { return GetGamePlayerControlData ( L, 57 ); }
int GetGamePlayerControlFlinchx ( lua_State *L ) { return GetGamePlayerControlData ( L, 58 ); }
int GetGamePlayerControlFlinchy ( lua_State *L ) { return GetGamePlayerControlData ( L, 59 ); }
int GetGamePlayerControlFlinchz ( lua_State *L ) { return GetGamePlayerControlData ( L, 60 ); }
int GetGamePlayerControlFlinchCurrentx ( lua_State *L ) { return GetGamePlayerControlData ( L, 61 ); }
int GetGamePlayerControlFlinchCurrenty ( lua_State *L ) { return GetGamePlayerControlData ( L, 62 ); }
int GetGamePlayerControlFlinchCurrentz ( lua_State *L ) { return GetGamePlayerControlData ( L, 63 ); }
int GetGamePlayerControlFootfallType ( lua_State *L ) { return GetGamePlayerControlData ( L, 64 ); }
int GetGamePlayerControlRippleCount ( lua_State *L ) { return GetGamePlayerControlData ( L, 65 ); }
int GetGamePlayerControlLastFootfallSound ( lua_State *L ) { return GetGamePlayerControlData ( L, 66 ); }
int GetGamePlayerControlInWaterState ( lua_State *L ) { return GetGamePlayerControlData ( L, 67 ); }
int GetGamePlayerControlDrownTimestamp ( lua_State *L ) { return GetGamePlayerControlData ( L, 68 ); }
int GetGamePlayerControlDeadTime ( lua_State *L ) { return GetGamePlayerControlData ( L, 69 ); }
int GetGamePlayerControlSwimTimestamp ( lua_State *L ) { return GetGamePlayerControlData ( L, 70 ); }
int GetGamePlayerControlRedDeathFog ( lua_State *L ) { return GetGamePlayerControlData ( L, 71 ); }
int GetGamePlayerControlHeartbeatTimeStamp ( lua_State *L ) { return GetGamePlayerControlData ( L, 72 ); }
int GetGamePlayerControlThirdpersonEnabled ( lua_State *L ) { return GetGamePlayerControlData ( L, 81 ); }
int GetGamePlayerControlThirdpersonCharacterIndex ( lua_State *L ) { return GetGamePlayerControlData ( L, 82 ); }
int GetGamePlayerControlThirdpersonCameraFollow ( lua_State *L ) { return GetGamePlayerControlData ( L, 83 ); }
int GetGamePlayerControlThirdpersonCameraFocus ( lua_State *L ) { return GetGamePlayerControlData ( L, 84 ); }
int GetGamePlayerControlThirdpersonCharactere ( lua_State *L ) { return GetGamePlayerControlData ( L, 85 ); }
int GetGamePlayerControlX7 ( lua_State *L ) { return GetGamePlayerControlData ( L, 86 ); }
int GetGamePlayerControlThirdpersonShotFired ( lua_State *L ) { return GetGamePlayerControlData ( L, 87 ); }
int GetGamePlayerControlThirdpersonCameraDistance ( lua_State *L ) { return GetGamePlayerControlData ( L, 88 ); }
int GetGamePlayerControlThirdpersonCameraSpeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 89 ); }
int GetGamePlayerControlThirdpersonCameraLocked ( lua_State *L ) { return GetGamePlayerControlData ( L, 90 ); }
int GetGamePlayerControlThirdpersonCameraHeight ( lua_State *L ) { return GetGamePlayerControlData ( L, 91 ); }
int GetGamePlayerControlThirdpersonCameraShoulder ( lua_State *L ) { return GetGamePlayerControlData ( L, 92 ); }
int GetGamePlayerControlFallDamageModifier(lua_State *L) { return GetGamePlayerControlData(L, 95); }
int GetGamePlayerControlSwimSpeed(lua_State *L) { return GetGamePlayerControlData(L, 96); }
int SetGamePlayerControlJetpackMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 1 ); }
int SetGamePlayerControlJetpackFuel ( lua_State *L ) { return SetGamePlayerControlData ( L, 2 ); }
int SetGamePlayerControlJetpackHidden ( lua_State *L ) { return SetGamePlayerControlData ( L, 3 ); }
int SetGamePlayerControlJetpackCollected ( lua_State *L ) { return SetGamePlayerControlData ( L, 4 ); }
int SetGamePlayerControlSoundStartIndex ( lua_State *L ) { return SetGamePlayerControlData ( L, 5 ); }
int SetGamePlayerControlJetpackParticleEmitterIndex ( lua_State *L ) { return SetGamePlayerControlData ( L, 6 ); }
int SetGamePlayerControlJetpackThrust ( lua_State *L ) { return SetGamePlayerControlData ( L, 7 ); }
int SetGamePlayerControlStartStrength ( lua_State *L ) { return SetGamePlayerControlData ( L, 8 ); }
int SetGamePlayerControlIsRunning ( lua_State *L ) { return SetGamePlayerControlData ( L, 9 ); }
int SetGamePlayerControlX6 ( lua_State *L ) { return SetGamePlayerControlData ( L, 10 ); }
int SetGamePlayerControlCx ( lua_State *L ) { return SetGamePlayerControlData ( L, 11 ); }
int SetGamePlayerControlCy ( lua_State *L ) { return SetGamePlayerControlData ( L, 12 ); }
int SetGamePlayerControlCz ( lua_State *L ) { return SetGamePlayerControlData ( L, 13 ); }
int SetGamePlayerControlBasespeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 14 ); }
int SetGamePlayerControlCanRun ( lua_State *L ) { return SetGamePlayerControlData ( L, 15 ); }
int SetGamePlayerControlMaxspeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 16 ); }
int SetGamePlayerControlTopspeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 17 ); }
int SetGamePlayerControlMovement ( lua_State *L ) { return SetGamePlayerControlData ( L, 18 ); }
int SetGamePlayerControlMovey ( lua_State *L ) { return SetGamePlayerControlData ( L, 19 ); }
int SetGamePlayerControlLastMovement ( lua_State *L ) { return SetGamePlayerControlData ( L, 20 ); }
int SetGamePlayerControlFootfallCount ( lua_State *L ) { return SetGamePlayerControlData ( L, 21 ); }
int SetGamePlayerControlX1 ( lua_State *L ) { return SetGamePlayerControlData ( L, 22 ); }
int SetGamePlayerControlGravityActive ( lua_State *L ) { return SetGamePlayerControlData ( L, 23 ); }
int SetGamePlayerControlPlrHitFloorMaterial ( lua_State *L ) { return SetGamePlayerControlData ( L, 24 ); }
int SetGamePlayerControlUnderwater ( lua_State *L ) { return SetGamePlayerControlData ( L, 25 ); }
int SetGamePlayerControlJumpMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 26 ); }
int SetGamePlayerControlJumpModeCanAffectVelocityCountdown ( lua_State *L ) { return SetGamePlayerControlData ( L, 27 ); }
int SetGamePlayerControlSpeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 28 ); }
int SetGamePlayerControlAccel ( lua_State *L ) { return SetGamePlayerControlData ( L, 29 ); }
int SetGamePlayerControlSpeedRatio ( lua_State *L ) { return SetGamePlayerControlData ( L, 30 ); }
int SetGamePlayerControlWobble ( lua_State *L ) { return SetGamePlayerControlData ( L, 31 ); }
int SetGamePlayerControlWobbleSpeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 32 ); }
int SetGamePlayerControlWobbleHeight ( lua_State *L ) { return SetGamePlayerControlData ( L, 33 ); }
int SetGamePlayerControlJumpmax ( lua_State *L ) { return SetGamePlayerControlData ( L, 34 ); }
int SetGamePlayerControlPushangle ( lua_State *L ) { return SetGamePlayerControlData ( L, 35 ); }
int SetGamePlayerControlPushforce ( lua_State *L ) { return SetGamePlayerControlData ( L, 36 ); }
int SetGamePlayerControlFootfallPace ( lua_State *L ) { return SetGamePlayerControlData ( L, 37 ); }
int SetGamePlayerControlLockAtHeight ( lua_State *L ) { return SetGamePlayerControlData ( L, 38 ); }
int SetGamePlayerControlControlHeight ( lua_State *L ) { return SetGamePlayerControlData ( L, 39 ); }
int SetGamePlayerControlControlHeightCooldown ( lua_State *L ) { return SetGamePlayerControlData ( L, 40 ); }
int SetGamePlayerControlStoreMovey ( lua_State *L ) { return SetGamePlayerControlData ( L, 41 ); }
int SetGamePlayerControlX3 ( lua_State *L ) { return SetGamePlayerControlData ( L, 42 ); }
int SetGamePlayerControlHurtFall ( lua_State *L ) { return SetGamePlayerControlData ( L, 43 ); }
int SetGamePlayerControlLeanoverAngle ( lua_State *L ) { return SetGamePlayerControlData ( L, 44 ); }
int SetGamePlayerControlLeanover ( lua_State *L ) { return SetGamePlayerControlData ( L, 45 ); }
int SetGamePlayerControlCameraShake ( lua_State *L ) { return SetGamePlayerControlData ( L, 46 ); }
int SetGamePlayerControlFinalCameraAnglex ( lua_State *L ) { return SetGamePlayerControlData ( L, 47 ); }
int SetGamePlayerControlFinalCameraAngley ( lua_State *L ) { return SetGamePlayerControlData ( L, 48 ); }
int SetGamePlayerControlFinalCameraAnglez ( lua_State *L ) { return SetGamePlayerControlData ( L, 49 ); }
int SetGamePlayerControlCamRightMouseMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 50 ); }
int SetGamePlayerControlCamCollisionSmooth ( lua_State *L ) { return SetGamePlayerControlData ( L, 51 ); }
int SetGamePlayerControlCamCurrentDistance ( lua_State *L ) { return SetGamePlayerControlData ( L, 52 ); }
int SetGamePlayerControlCamDoFullRayCheck ( lua_State *L ) { return SetGamePlayerControlData ( L, 53 ); }
int SetGamePlayerControlLastGoodcx ( lua_State *L ) { return SetGamePlayerControlData ( L, 54 ); }
int SetGamePlayerControlLastGoodcy ( lua_State *L ) { return SetGamePlayerControlData ( L, 55 ); }
int SetGamePlayerControlLastGoodcz ( lua_State *L ) { return SetGamePlayerControlData ( L, 56 ); }
int SetGamePlayerControlX4 ( lua_State *L ) { return SetGamePlayerControlData ( L, 57 ); }
int SetGamePlayerControlFlinchx ( lua_State *L ) { return SetGamePlayerControlData ( L, 58 ); }
int SetGamePlayerControlFlinchy ( lua_State *L ) { return SetGamePlayerControlData ( L, 59 ); }
int SetGamePlayerControlFlinchz ( lua_State *L ) { return SetGamePlayerControlData ( L, 60 ); }
int SetGamePlayerControlFlinchCurrentx ( lua_State *L ) { return SetGamePlayerControlData ( L, 61 ); }
int SetGamePlayerControlFlinchCurrenty ( lua_State *L ) { return SetGamePlayerControlData ( L, 62 ); }
int SetGamePlayerControlFlinchCurrentz ( lua_State *L ) { return SetGamePlayerControlData ( L, 63 ); }
int SetGamePlayerControlFootfallType ( lua_State *L ) { return SetGamePlayerControlData ( L, 64 ); }
int SetGamePlayerControlRippleCount ( lua_State *L ) { return SetGamePlayerControlData ( L, 65 ); }
int SetGamePlayerControlLastFootfallSound ( lua_State *L ) { return SetGamePlayerControlData ( L, 66 ); }
int SetGamePlayerControlInWaterState ( lua_State *L ) { return SetGamePlayerControlData ( L, 67 ); }
int SetGamePlayerControlDrownTimestamp ( lua_State *L ) { return SetGamePlayerControlData ( L, 68 ); }
int SetGamePlayerControlDeadTime ( lua_State *L ) { return SetGamePlayerControlData ( L, 69 ); }
int SetGamePlayerControlSwimTimestamp ( lua_State *L ) { return SetGamePlayerControlData ( L, 70 ); }
int SetGamePlayerControlRedDeathFog ( lua_State *L ) { return SetGamePlayerControlData ( L, 71 ); }
int SetGamePlayerControlHeartbeatTimeStamp ( lua_State *L ) { return SetGamePlayerControlData ( L, 72 ); }
int SetGamePlayerControlThirdpersonEnabled ( lua_State *L ) { return SetGamePlayerControlData ( L, 81 ); }
int SetGamePlayerControlThirdpersonCharacterIndex ( lua_State *L ) { return SetGamePlayerControlData ( L, 82 ); }
int SetGamePlayerControlThirdpersonCameraFollow ( lua_State *L ) { return SetGamePlayerControlData ( L, 83 ); }
int SetGamePlayerControlThirdpersonCameraFocus ( lua_State *L ) { return SetGamePlayerControlData ( L, 84 ); }
int SetGamePlayerControlThirdpersonCharactere ( lua_State *L ) { return SetGamePlayerControlData ( L, 85 ); }
int SetGamePlayerControlX7 ( lua_State *L ) { return SetGamePlayerControlData ( L, 86 ); }
int SetGamePlayerControlThirdpersonShotFired ( lua_State *L ) { return SetGamePlayerControlData ( L, 87 ); }
int SetGamePlayerControlThirdpersonCameraDistance ( lua_State *L ) { return SetGamePlayerControlData ( L, 88 ); }
int SetGamePlayerControlThirdpersonCameraSpeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 89 ); }
int SetGamePlayerControlThirdpersonCameraLocked ( lua_State *L ) { return SetGamePlayerControlData ( L, 90 ); }
int SetGamePlayerControlThirdpersonCameraHeight ( lua_State *L ) { return SetGamePlayerControlData ( L, 91 ); }
int SetGamePlayerControlThirdpersonCameraShoulder ( lua_State *L ) { return SetGamePlayerControlData ( L, 92 ); }

// 99 used
int SetGamePlayerStateGunMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 101 ); }
int GetGamePlayerStateGunMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 101 ); }
int SetGamePlayerStateFiringMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 102 ); }
int GetGamePlayerStateFiringMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 102 ); }
int GetGamePlayerStateWeaponAmmoIndex ( lua_State *L ) { return GetGamePlayerControlData ( L, 103 ); }
int GetGamePlayerStateAmmoOffset ( lua_State *L ) { return GetGamePlayerControlData ( L, 104 ); }
int GetGamePlayerStateGunMeleeKey ( lua_State *L ) { return GetGamePlayerControlData ( L, 105 ); }
int SetGamePlayerStateBlockingAction ( lua_State *L ) { return SetGamePlayerControlData ( L, 106 ); }
int GetGamePlayerStateBlockingAction (lua_State* L) { return GetGamePlayerControlData (L, 106); }
int SetGamePlayerStateGunShootNoAmmo ( lua_State *L ) { return SetGamePlayerControlData ( L, 107 ); }
int GetGamePlayerStateGunShootNoAmmo ( lua_State *L ) { return GetGamePlayerControlData ( L, 107 ); }
int SetGamePlayerStateUnderwater ( lua_State *L ) { return SetGamePlayerControlData ( L, 108 ); }
int GetGamePlayerStateUnderwater ( lua_State *L ) { return GetGamePlayerControlData ( L, 108 ); }
int SetGamePlayerStateRightMouseHold ( lua_State *L ) { return SetGamePlayerControlData ( L, 109 ); }
int GetGamePlayerStateRightMouseHold ( lua_State *L ) { return GetGamePlayerControlData ( L, 109 ); }
int SetGamePlayerStateXBOX ( lua_State *L ) { return SetGamePlayerControlData ( L, 110 ); }
int GetGamePlayerStateXBOX ( lua_State *L ) { return GetGamePlayerControlData ( L, 110 ); }
int SetGamePlayerStateXBOXControllerType ( lua_State *L ) { return SetGamePlayerControlData ( L, 99 ); }
int GetGamePlayerStateXBOXControllerType ( lua_State *L ) { return GetGamePlayerControlData ( L, 99 ); }
int JoystickX ( lua_State *L ) { return GetGamePlayerControlData ( L, 111 ); }
int JoystickY ( lua_State *L ) { return GetGamePlayerControlData ( L, 112 ); }
int JoystickZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 113 ); }
int SetGamePlayerStateGunZoomMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 114 ); }
int GetGamePlayerStateGunZoomMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 114 ); }
int SetGamePlayerStateGunZoomMag ( lua_State *L ) { return SetGamePlayerControlData ( L, 115 ); }
int GetGamePlayerStateGunZoomMag ( lua_State *L ) { return GetGamePlayerControlData ( L, 115 ); }
int SetGamePlayerStateGunReloadNoAmmo ( lua_State *L ) { return SetGamePlayerControlData ( L, 116 ); }
int GetGamePlayerStateGunReloadNoAmmo ( lua_State *L ) { return GetGamePlayerControlData ( L, 116 ); }
int SetGamePlayerStatePlrReloading ( lua_State *L ) { return SetGamePlayerControlData ( L, 117 ); }
int GetGamePlayerStatePlrReloading ( lua_State *L ) { return GetGamePlayerControlData ( L, 117 ); }
int SetGamePlayerStateGunAltSwapKey1 ( lua_State *L ) { return SetGamePlayerControlData ( L, 118 ); }
int GetGamePlayerStateGunAltSwapKey1 ( lua_State *L ) { return GetGamePlayerControlData ( L, 118 ); }
int SetGamePlayerStateGunAltSwapKey2 ( lua_State *L ) { return SetGamePlayerControlData ( L, 119 ); }
int GetGamePlayerStateGunAltSwapKey2 ( lua_State *L ) { return GetGamePlayerControlData ( L, 119 ); }
int SetGamePlayerStateWeaponKeySelection ( lua_State *L ) { return SetGamePlayerControlData ( L, 120 ); }
int GetGamePlayerStateWeaponKeySelection ( lua_State *L ) { return GetGamePlayerControlData ( L, 120 ); }
int SetGamePlayerStateWeaponIndex ( lua_State *L ) { return SetGamePlayerControlData ( L, 121 ); }
int GetGamePlayerStateWeaponIndex ( lua_State *L ) { return GetGamePlayerControlData ( L, 121 ); }
int SetGamePlayerStateCommandNewWeapon ( lua_State *L ) { return SetGamePlayerControlData ( L, 122 ); }
int GetGamePlayerStateCommandNewWeapon ( lua_State *L ) { return GetGamePlayerControlData ( L, 122 ); }
int SetGamePlayerStateGunID ( lua_State *L ) { return SetGamePlayerControlData ( L, 123 ); }
int GetGamePlayerStateGunID ( lua_State *L ) { return GetGamePlayerControlData ( L, 123 ); }
int SetGamePlayerStateGunSelectionAfterHide ( lua_State *L ) { return SetGamePlayerControlData ( L, 124 ); }
int GetGamePlayerStateGunSelectionAfterHide ( lua_State *L ) { return GetGamePlayerControlData ( L, 124 ); }
int SetGamePlayerStateGunBurst ( lua_State *L ) { return SetGamePlayerControlData ( L, 125 ); }
int GetGamePlayerStateGunBurst ( lua_State *L ) { return GetGamePlayerControlData ( L, 125 ); }
int SetGamePlayerStatePlrKeyForceKeystate (lua_State *L) { return SetGamePlayerControlData (L, 129); }
int JoystickHatAngle ( lua_State *L ) { return GetGamePlayerControlData ( L, 503 ); }
int JoystickFireXL ( lua_State *L ) { return GetGamePlayerControlData ( L, 504 ); }
int JoystickTwistX ( lua_State *L ) { return GetGamePlayerControlData ( L, 127 ); }
int JoystickTwistY ( lua_State *L ) { return GetGamePlayerControlData ( L, 128 ); }
int JoystickTwistZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 129 ); }
int SetGamePlayerStatePlrZoomInChange ( lua_State *L ) { return SetGamePlayerControlData ( L, 130 ); }
int GetGamePlayerStatePlrZoomInChange ( lua_State *L ) { return GetGamePlayerControlData ( L, 130 ); }
int SetGamePlayerStatePlrZoomIn ( lua_State *L ) { return SetGamePlayerControlData ( L, 131 ); }
int GetGamePlayerStatePlrZoomIn ( lua_State *L ) { return GetGamePlayerControlData ( L, 131 ); }
int SetGamePlayerStateLuaActiveMouse ( lua_State *L ) { return SetGamePlayerControlData ( L, 132 ); }
int GetGamePlayerStateLuaActiveMouse ( lua_State *L ) { return GetGamePlayerControlData ( L, 132 ); }
int SetGamePlayerStateRealFov ( lua_State *L ) { return SetGamePlayerControlData ( L, 133 ); }
int GetGamePlayerStateRealFov ( lua_State *L ) { return GetGamePlayerControlData ( L, 133 ); }
int SetGamePlayerStateDisablePeeking ( lua_State *L ) { return SetGamePlayerControlData ( L, 134 ); }
int GetGamePlayerStateDisablePeeking ( lua_State *L ) { return GetGamePlayerControlData ( L, 134 ); }
int SetGamePlayerStatePlrHasFocus ( lua_State *L ) { return SetGamePlayerControlData ( L, 135 ); }
int GetGamePlayerStatePlrHasFocus ( lua_State *L ) { return GetGamePlayerControlData ( L, 135 ); }
int SetGamePlayerStateGameRunAsMultiplayer ( lua_State *L ) { return SetGamePlayerControlData ( L, 136 ); }
int GetGamePlayerStateGameRunAsMultiplayer ( lua_State *L ) { return GetGamePlayerControlData ( L, 136 ); }
int SetGamePlayerStateSteamWorksRespawnLeft ( lua_State *L ) { return SetGamePlayerControlData ( L, 137 ); }
int GetGamePlayerStateSteamWorksRespawnLeft ( lua_State *L ) { return GetGamePlayerControlData ( L, 137 ); }
int SetGamePlayerStateMPRespawnLeft ( lua_State *L ) { return SetGamePlayerControlData ( L, 137 ); }
int GetGamePlayerStateMPRespawnLeft ( lua_State *L ) { return GetGamePlayerControlData ( L, 137 ); }
int SetGamePlayerStateTabMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 138 ); }
int GetGamePlayerStateTabMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 138 ); }
int SetGamePlayerStateLowFpsWarning ( lua_State *L ) { return SetGamePlayerControlData ( L, 139 ); }
int GetGamePlayerStateLowFpsWarning ( lua_State *L ) { return GetGamePlayerControlData ( L, 139 ); }
int SetGamePlayerStateCameraFov ( lua_State *L ) { return SetGamePlayerControlData ( L, 140 ); }
int GetGamePlayerStateCameraFov ( lua_State *L ) { return GetGamePlayerControlData ( L, 140 ); }
int SetGamePlayerStateCameraFovZoomed ( lua_State *L ) { return SetGamePlayerControlData ( L, 141 ); }
int GetGamePlayerStateCameraFovZoomed ( lua_State *L ) { return GetGamePlayerControlData ( L, 141 ); }
int SetGamePlayerStateMouseInvert ( lua_State *L ) { return SetGamePlayerControlData ( L, 142 ); }
int GetGamePlayerStateMouseInvert ( lua_State *L ) { return GetGamePlayerControlData ( L, 142 ); }
int SetGamePlayerStateSlowMotion ( lua_State *L ) { return SetGamePlayerControlData ( L, 143 ); }
int GetGamePlayerStateSlowMotion ( lua_State *L ) { return GetGamePlayerControlData ( L, 143 ); }
int SetGamePlayerStateSmoothCameraKeys ( lua_State *L ) { return SetGamePlayerControlData ( L, 144 ); }
int GetGamePlayerStateSmoothCameraKeys ( lua_State *L ) { return GetGamePlayerControlData ( L, 144 ); }
int SetGamePlayerStateCamMouseMoveX ( lua_State *L ) { return SetGamePlayerControlData ( L, 145 ); }
int GetGamePlayerStateCamMouseMoveX ( lua_State *L ) { return GetGamePlayerControlData ( L, 145 ); }
int SetGamePlayerStateCamMouseMoveY ( lua_State *L ) { return SetGamePlayerControlData ( L, 146 ); }
int GetGamePlayerStateCamMouseMoveY ( lua_State *L ) { return GetGamePlayerControlData ( L, 146 ); }
int SetGamePlayerStateGunRecoilX ( lua_State *L ) { return SetGamePlayerControlData ( L, 147 ); }
int GetGamePlayerStateGunRecoilX ( lua_State *L ) { return GetGamePlayerControlData ( L, 147 ); }
int SetGamePlayerStateGunRecoilY ( lua_State *L ) { return SetGamePlayerControlData ( L, 148 ); }
int GetGamePlayerStateGunRecoilY ( lua_State *L ) { return GetGamePlayerControlData ( L, 148 ); }
int SetGamePlayerStateGunRecoilAngleX ( lua_State *L ) { return SetGamePlayerControlData ( L, 149 ); }
int GetGamePlayerStateGunRecoilAngleX ( lua_State *L ) { return GetGamePlayerControlData ( L, 149 ); }
int SetGamePlayerStateGunRecoilAngleY ( lua_State *L ) { return SetGamePlayerControlData ( L, 150 ); }
int GetGamePlayerStateGunRecoilAngleY ( lua_State *L ) { return GetGamePlayerControlData ( L, 150 ); }
int SetGamePlayerStateGunRecoilCorrectY ( lua_State *L ) { return SetGamePlayerControlData ( L, 151 ); }
int GetGamePlayerStateGunRecoilCorrectY ( lua_State *L ) { return GetGamePlayerControlData ( L, 151 ); }
int SetGamePlayerStateGunRecoilCorrectX ( lua_State *L ) { return SetGamePlayerControlData ( L, 152 ); }
int GetGamePlayerStateGunRecoilCorrectX ( lua_State *L ) { return GetGamePlayerControlData ( L, 152 ); }
int SetGamePlayerStateGunRecoilCorrectAngleY ( lua_State *L ) { return SetGamePlayerControlData ( L, 153 ); }
int GetGamePlayerStateGunRecoilCorrectAngleY ( lua_State *L ) { return GetGamePlayerControlData ( L, 153 ); }
int SetGamePlayerStateGunRecoilCorrectAngleX ( lua_State *L ) { return SetGamePlayerControlData ( L, 154 ); }
int GetGamePlayerStateGunRecoilCorrectAngleX ( lua_State *L ) { return GetGamePlayerControlData ( L, 154 ); }
int SetGamePlayerStateCamAngleX ( lua_State *L ) { return SetGamePlayerControlData ( L, 155 ); }
int GetGamePlayerStateCamAngleX ( lua_State *L ) { return GetGamePlayerControlData ( L, 155 ); }
int SetGamePlayerStateCamAngleY ( lua_State *L ) { return SetGamePlayerControlData ( L, 156 ); }
int GetGamePlayerStateCamAngleY ( lua_State *L ) { return GetGamePlayerControlData ( L, 156 ); }
int SetGamePlayerStatePlayerDucking ( lua_State *L ) { return SetGamePlayerControlData ( L, 157 ); }
int GetGamePlayerStatePlayerDucking ( lua_State *L ) { return GetGamePlayerControlData ( L, 157 ); }
int SetGamePlayerStateEditModeActive ( lua_State *L ) { return SetGamePlayerControlData ( L, 158 ); }
int GetGamePlayerStateEditModeActive ( lua_State *L ) { return GetGamePlayerControlData ( L, 158 ); }
int SetGamePlayerStatePlrKeyShift ( lua_State *L ) { return SetGamePlayerControlData ( L, 159 ); }
int GetGamePlayerStatePlrKeyShift ( lua_State *L ) { return GetGamePlayerControlData ( L, 159 ); }
int SetGamePlayerStatePlrKeyShift2 ( lua_State *L ) { return SetGamePlayerControlData ( L, 160 ); }
int GetGamePlayerStatePlrKeyShift2 ( lua_State *L ) { return GetGamePlayerControlData ( L, 160 ); }
int SetGamePlayerStatePlrKeyControl ( lua_State *L ) { return SetGamePlayerControlData ( L, 161 ); }
int GetGamePlayerStatePlrKeyControl ( lua_State *L ) { return GetGamePlayerControlData ( L, 161 ); }
int SetGamePlayerStateNoWater ( lua_State *L ) { return SetGamePlayerControlData ( L, 162 ); }
int GetGamePlayerStateNoWater ( lua_State *L ) { return GetGamePlayerControlData ( L, 162 ); }
int SetGamePlayerStateWaterlineY ( lua_State *L ) { return SetGamePlayerControlData ( L, 163 ); }
int GetGamePlayerStateWaterlineY ( lua_State *L ) { return GetGamePlayerControlData ( L, 163 ); }
int SetGamePlayerStateFlashlightKeyEnabled ( lua_State *L ) { return SetGamePlayerControlData ( L, 164 ); }
int GetGamePlayerStateFlashlightKeyEnabled ( lua_State *L ) { return GetGamePlayerControlData ( L, 164 ); }
int SetGamePlayerStateFlashlightControl ( lua_State *L ) { return SetGamePlayerControlData ( L, 165 ); }
int GetGamePlayerStateFlashlightControl ( lua_State *L ) { return GetGamePlayerControlData ( L, 165 ); }
int SetGamePlayerStateMoving ( lua_State *L ) { return SetGamePlayerControlData ( L, 166 ); }
int GetGamePlayerStateMoving ( lua_State *L ) { return GetGamePlayerControlData ( L, 166 ); }
int SetGamePlayerStateTerrainHeight ( lua_State *L ) { return SetGamePlayerControlData ( L, 167 ); }
int GetGamePlayerStateTerrainHeight ( lua_State *L ) { return GetGamePlayerControlData ( L, 167 ); }
int SetGamePlayerStateJetpackVerticalMove ( lua_State *L ) { return SetGamePlayerControlData ( L, 168 ); }
int GetGamePlayerStateJetpackVerticalMove ( lua_State *L ) { return GetGamePlayerControlData ( L, 168 ); }
int SetGamePlayerStateTerrainID ( lua_State *L ) { return SetGamePlayerControlData ( L, 169 ); }
int GetGamePlayerStateTerrainID ( lua_State *L ) { return GetGamePlayerControlData ( L, 169 ); }
int SetGamePlayerStateEnablePlrSpeedMods ( lua_State *L ) { return SetGamePlayerControlData ( L, 170 ); }
int GetGamePlayerStateEnablePlrSpeedMods ( lua_State *L ) { return GetGamePlayerControlData ( L, 170 ); }
int SetGamePlayerStateRiftMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 171 ); }
int GetGamePlayerStateRiftMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 171 ); }
int SetGamePlayerStatePlayerX ( lua_State *L ) { return SetGamePlayerControlData ( L, 172 ); }
int GetGamePlayerStatePlayerX ( lua_State *L ) { return GetGamePlayerControlData ( L, 172 ); }
int SetGamePlayerStatePlayerY ( lua_State *L ) { return SetGamePlayerControlData ( L, 173 ); }
int GetGamePlayerStatePlayerY ( lua_State *L ) { return GetGamePlayerControlData ( L, 173 ); }
int SetGamePlayerStatePlayerZ ( lua_State *L ) { return SetGamePlayerControlData ( L, 174 ); }
int GetGamePlayerStatePlayerZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 174 ); }
int SetGamePlayerStateTerrainPlayerX ( lua_State *L ) { return SetGamePlayerControlData ( L, 175 ); }
int GetGamePlayerStateTerrainPlayerX ( lua_State *L ) { return GetGamePlayerControlData ( L, 175 ); }
int SetGamePlayerStateTerrainPlayerY ( lua_State *L ) { return SetGamePlayerControlData ( L, 176 ); }
int GetGamePlayerStateTerrainPlayerY ( lua_State *L ) { return GetGamePlayerControlData ( L, 176 ); }
int SetGamePlayerStateTerrainPlayerZ ( lua_State *L ) { return SetGamePlayerControlData ( L, 177 ); }
int GetGamePlayerStateTerrainPlayerZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 177 ); }
int SetGamePlayerStateTerrainPlayerAX ( lua_State *L ) { return SetGamePlayerControlData ( L, 178 ); }
int GetGamePlayerStateTerrainPlayerAX ( lua_State *L ) { return GetGamePlayerControlData ( L, 178 ); }
int SetGamePlayerStateTerrainPlayerAY ( lua_State *L ) { return SetGamePlayerControlData ( L, 179 ); }
int GetGamePlayerStateTerrainPlayerAY ( lua_State *L ) { return GetGamePlayerControlData ( L, 179 ); }
int SetGamePlayerStateTerrainPlayerAZ ( lua_State *L ) { return SetGamePlayerControlData ( L, 180 ); }
int GetGamePlayerStateTerrainPlayerAZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 180 ); }
int SetGamePlayerStateAdjustBasedOnWobbleY ( lua_State *L ) { return SetGamePlayerControlData ( L, 181 ); }
int GetGamePlayerStateAdjustBasedOnWobbleY ( lua_State *L ) { return GetGamePlayerControlData ( L, 181 ); }
int SetGamePlayerStateFinalCamX ( lua_State *L ) { return SetGamePlayerControlData ( L, 182 ); }
int GetGamePlayerStateFinalCamX ( lua_State *L ) { return GetGamePlayerControlData ( L, 182 ); }
int SetGamePlayerStateFinalCamY ( lua_State *L ) { return SetGamePlayerControlData ( L, 183 ); }
int GetGamePlayerStateFinalCamY ( lua_State *L ) { return GetGamePlayerControlData ( L, 183 ); }
int SetGamePlayerStateFinalCamZ ( lua_State *L ) { return SetGamePlayerControlData ( L, 184 ); }
int GetGamePlayerStateFinalCamZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 184 ); }
int SetGamePlayerStateShakeX ( lua_State *L ) { return SetGamePlayerControlData ( L, 185 ); }
int GetGamePlayerStateShakeX ( lua_State *L ) { return GetGamePlayerControlData ( L, 185 ); }
int SetGamePlayerStateShakeY ( lua_State *L ) { return SetGamePlayerControlData ( L, 186 ); }
int GetGamePlayerStateShakeY ( lua_State *L ) { return GetGamePlayerControlData ( L, 186 ); }
int SetGamePlayerStateShakeZ ( lua_State *L ) { return SetGamePlayerControlData ( L, 187 ); }
int GetGamePlayerStateShakeZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 187 ); }
int SetGamePlayerStateImmunity ( lua_State *L ) { return SetGamePlayerControlData ( L, 188 ); }
int GetGamePlayerStateImmunity ( lua_State *L ) { return GetGamePlayerControlData ( L, 188 ); }
int SetGamePlayerStateCharAnimIndex ( lua_State *L ) { return SetGamePlayerControlData ( L, 189 ); }
int GetGamePlayerStateCharAnimIndex ( lua_State *L ) { return GetGamePlayerControlData ( L, 189 ); }

int GetGamePlayerStateMotionController ( lua_State *L ) { return GetGamePlayerControlData ( L, 190 ); }
int GetGamePlayerStateMotionControllerType ( lua_State *L ) { return GetGamePlayerControlData ( L, 191 ); }
int MotionControllerThumbstickX ( lua_State *L ) { return GetGamePlayerControlData ( L, 192 ); }
int MotionControllerThumbstickY ( lua_State *L ) { return GetGamePlayerControlData ( L, 193 ); }
int CombatControllerTrigger ( lua_State *L ) { return GetGamePlayerControlData ( L, 194 ); }
int CombatControllerGrip ( lua_State *L ) { return GetGamePlayerControlData ( L, 195 ); }
int CombatControllerThumbstickX ( lua_State *L ) { return GetGamePlayerControlData ( L, 196 ); }
int CombatControllerThumbstickY ( lua_State *L ) { return GetGamePlayerControlData ( L, 197 ); }
int CombatControllerButtonA (lua_State* L) { return GetGamePlayerControlData (L, 198); }
int CombatControllerButtonB (lua_State* L) { return GetGamePlayerControlData (L, 199); }

int SetGamePlayerStateFlashlightRange (lua_State *L) { return SetGamePlayerControlData (L, 401); }
int GetGamePlayerStateFlashlightRange (lua_State *L) { return GetGamePlayerControlData (L, 401); }
int SetGamePlayerStateFlashlightRadius (lua_State *L) { return SetGamePlayerControlData (L, 402); }
int GetGamePlayerStateFlashlightRadius (lua_State *L) { return GetGamePlayerControlData (L, 402); }
int SetGamePlayerStateFlashlightColorR (lua_State *L) { return SetGamePlayerControlData (L, 403); }
int GetGamePlayerStateFlashlightColorR (lua_State *L) { return GetGamePlayerControlData (L, 403); }
int SetGamePlayerStateFlashlightColorG (lua_State *L) { return SetGamePlayerControlData (L, 404); }
int GetGamePlayerStateFlashlightColorG (lua_State *L) { return GetGamePlayerControlData (L, 404); }
int SetGamePlayerStateFlashlightColorB (lua_State *L) { return SetGamePlayerControlData (L, 405); }
int GetGamePlayerStateFlashlightColorB (lua_State *L) { return GetGamePlayerControlData (L, 405); }
int SetGamePlayerStateFlashlightCastShadow (lua_State *L) { return SetGamePlayerControlData (L, 406); }
int GetGamePlayerStateFlashlightCastShadow (lua_State *L) { return GetGamePlayerControlData (L, 406); }

int MotionControllerBestX ( lua_State *L ) { return GetGamePlayerControlData ( L, 251 ); }
int MotionControllerBestY ( lua_State *L ) { return GetGamePlayerControlData ( L, 252 ); }
int MotionControllerBestZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 253 ); }
int MotionControllerBestAngleX ( lua_State *L ) { return GetGamePlayerControlData ( L, 254 ); }
int MotionControllerBestAngleY ( lua_State *L ) { return GetGamePlayerControlData ( L, 255 ); }
int MotionControllerBestAngleZ ( lua_State *L ) { return GetGamePlayerControlData ( L, 256 ); }
int MotionControllerLaserGuidedEntityObj ( lua_State *L ) { return GetGamePlayerControlData ( L, 257 ); }

int SetGamePlayerStateIsMelee ( lua_State *L ) { return SetGamePlayerControlData ( L, 201 ); }
int GetGamePlayerStateIsMelee ( lua_State *L ) { return GetGamePlayerControlData ( L, 201 ); }
int SetGamePlayerStateAlternate ( lua_State *L ) { return SetGamePlayerControlData ( L, 202 ); }
int GetGamePlayerStateAlternate ( lua_State *L ) { return GetGamePlayerControlData ( L, 202 ); }
int SetGamePlayerStateModeShareMags ( lua_State *L ) { return SetGamePlayerControlData ( L, 203 ); }
int GetGamePlayerStateModeShareMags ( lua_State *L ) { return GetGamePlayerControlData ( L, 203 ); }
int SetGamePlayerStateAlternateIsFlak ( lua_State *L ) { return SetGamePlayerControlData ( L, 204 ); }
int GetGamePlayerStateAlternateIsFlak ( lua_State *L ) { return GetGamePlayerControlData ( L, 204 ); }
int SetGamePlayerStateAlternateIsRay ( lua_State *L ) { return SetGamePlayerControlData ( L, 205 ); }
int GetGamePlayerStateAlternateIsRay ( lua_State *L ) { return GetGamePlayerControlData ( L, 205 ); }
int SetFireModeSettingsReloadQty ( lua_State *L ) { return SetGamePlayerControlData ( L, 301 ); }
int GetFireModeSettingsReloadQty ( lua_State *L ) { return GetGamePlayerControlData ( L, 301 ); }
int SetFireModeSettingsIsEmpty ( lua_State *L ) { return SetGamePlayerControlData ( L, 302 ); }
int GetFireModeSettingsIsEmpty ( lua_State *L ) { return GetGamePlayerControlData ( L, 302 ); }
int SetFireModeSettingsJammed ( lua_State *L ) { return SetGamePlayerControlData ( L, 303 ); }
int GetFireModeSettingsJammed ( lua_State *L ) { return GetGamePlayerControlData ( L, 303 ); }
int SetFireModeSettingsJamChance ( lua_State *L ) { return SetGamePlayerControlData ( L, 304 ); }
int GetFireModeSettingsJamChance ( lua_State *L ) { return GetGamePlayerControlData ( L, 304 ); }
int SetFireModeSettingsMinTimer ( lua_State *L ) { return SetGamePlayerControlData ( L, 305 ); }
int GetFireModeSettingsMinTimer ( lua_State *L ) { return GetGamePlayerControlData ( L, 305 ); }
int SetFireModeSettingsAddTimer ( lua_State *L ) { return SetGamePlayerControlData ( L, 306 ); }
int GetFireModeSettingsAddTimer ( lua_State *L ) { return GetGamePlayerControlData ( L, 306 ); }
int SetFireModeSettingsShotsFired ( lua_State *L ) { return SetGamePlayerControlData ( L, 307 ); }
int GetFireModeSettingsShotsFired ( lua_State *L ) { return GetGamePlayerControlData ( L, 307 ); }
int SetFireModeSettingsCoolTimer ( lua_State *L ) { return SetGamePlayerControlData ( L, 308 ); }
int GetFireModeSettingsCoolTimer ( lua_State *L ) { return GetGamePlayerControlData ( L, 308 ); }
int SetFireModeSettingsOverheatAfter ( lua_State *L ) { return SetGamePlayerControlData ( L, 309 ); }
int GetFireModeSettingsOverheatAfter ( lua_State *L ) { return GetGamePlayerControlData ( L, 309 ); }
int SetFireModeSettingsJamChanceTime ( lua_State *L ) { return SetGamePlayerControlData ( L, 310 ); }
int GetFireModeSettingsJamChanceTime ( lua_State *L ) { return GetGamePlayerControlData ( L, 310 ); }
int SetFireModeSettingsCoolDown ( lua_State *L ) { return SetGamePlayerControlData ( L, 311 ); }
int GetFireModeSettingsCoolDown ( lua_State *L ) { return GetGamePlayerControlData ( L, 311 ); }
int SetFireModeSettingsNoSubmergedFire ( lua_State *L ) { return SetGamePlayerControlData ( L, 312 ); }
int GetFireModeSettingsNoSubmergedFire ( lua_State *L ) { return GetGamePlayerControlData ( L, 312 ); }
int SetFireModeSettingsSimpleZoom ( lua_State *L ) { return SetGamePlayerControlData ( L, 313 ); }
int GetFireModeSettingsSimpleZoom ( lua_State *L ) { return GetGamePlayerControlData ( L, 313 ); }
int SetFireModeSettingsForceZoomOut ( lua_State *L ) { return SetGamePlayerControlData ( L, 314 ); }
int GetFireModeSettingsForceZoomOut ( lua_State *L ) { return GetGamePlayerControlData ( L, 314 ); }
int SetFireModeSettingsZoomMode ( lua_State *L ) { return SetGamePlayerControlData ( L, 315 ); }
int GetFireModeSettingsZoomMode ( lua_State *L ) { return GetGamePlayerControlData ( L, 315 ); }
int SetFireModeSettingsSimpleZoomAnim ( lua_State *L ) { return SetGamePlayerControlData ( L, 316 ); }
int GetFireModeSettingsSimpleZoomAnim ( lua_State *L ) { return GetGamePlayerControlData ( L, 316 ); }
int SetFireModeSettingsPoolIndex ( lua_State *L ) { return SetGamePlayerControlData ( L, 317 ); }
int GetFireModeSettingsPoolIndex ( lua_State *L ) { return GetGamePlayerControlData ( L, 317 ); }
int SetFireModeSettingsPlrTurnSpeedMod ( lua_State *L ) { return SetGamePlayerControlData ( L, 318 ); }
int GetFireModeSettingsPlrTurnSpeedMod ( lua_State *L ) { return GetGamePlayerControlData ( L, 318 ); }
int SetFireModeSettingsZoomTurnSpeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 319 ); }
int GetFireModeSettingsZoomTurnSpeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 319 ); }
int SetFireModeSettingsPlrJumpSpeedMod ( lua_State *L ) { return SetGamePlayerControlData ( L, 320 ); }
int GetFireModeSettingsPlrJumpSpeedMod ( lua_State *L ) { return GetGamePlayerControlData ( L, 320 ); }
int SetFireModeSettingsPlrEmptySpeedMod ( lua_State *L ) { return SetGamePlayerControlData ( L, 321 ); }
int GetFireModeSettingsPlrEmptySpeedMod ( lua_State *L ) { return GetGamePlayerControlData ( L, 321 ); }
int SetFireModeSettingsPlrMoveSpeedMod ( lua_State *L ) { return SetGamePlayerControlData ( L, 322 ); }
int GetFireModeSettingsPlrMoveSpeedMod ( lua_State *L ) { return GetGamePlayerControlData ( L, 322 ); }
int SetFireModeSettingsZoomWalkSpeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 323 ); }
int GetFireModeSettingsZoomWalkSpeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 323 ); }
int SetFireModeSettingsPlrReloadSpeedMod ( lua_State *L ) { return SetGamePlayerControlData ( L, 324 ); }
int GetFireModeSettingsPlrReloadSpeedMod ( lua_State *L ) { return GetGamePlayerControlData ( L, 324 ); }
int SetFireModeSettingsHasEmpty ( lua_State *L ) { return SetGamePlayerControlData ( L, 325 ); }
int GetFireModeSettingsHasEmpty ( lua_State *L ) { return GetGamePlayerControlData ( L, 325 ); }
int GetFireModeSettingsActionBlockStart ( lua_State *L ) { return GetGamePlayerControlData ( L, 326 ); }
int GetFireModeSettingsMeleeWithRightClick (lua_State *L) { return GetGamePlayerControlData (L, 327); }
int GetFireModeSettingsBlockWithRightClick (lua_State* L) { return GetGamePlayerControlData (L, 328); }

int SetGamePlayerStateGunSound ( lua_State *L ) { return SetGamePlayerControlData ( L, 501 ); }
int GetGamePlayerStateGunSound ( lua_State *L ) { return GetGamePlayerControlData ( L, 501 ); }
int SetGamePlayerStateGunAltSound ( lua_State *L ) { return SetGamePlayerControlData ( L, 502 ); }
int GetGamePlayerStateGunAltSound ( lua_State *L ) { return GetGamePlayerControlData ( L, 502 ); }

int SetGamePlayerStateCounteredAction (lua_State* L) { return SetGamePlayerControlData (L, 601); }
int GetGamePlayerStateCounteredAction (lua_State* L) { return GetGamePlayerControlData (L, 601); }

int CopyCharAnimState ( lua_State *L ) { return SetGamePlayerControlData ( L, 700 ); }
int SetCharAnimStatePlayCsi ( lua_State *L ) { return SetGamePlayerControlData ( L, 701 ); }
int GetCharAnimStatePlayCsi ( lua_State *L ) { return GetGamePlayerControlData ( L, 701 ); }
int SetCharAnimStateOriginalE ( lua_State *L ) { return SetGamePlayerControlData ( L, 702 ); }
int GetCharAnimStateOriginalE ( lua_State *L ) { return GetGamePlayerControlData ( L, 702 ); }
int SetCharAnimStateObj ( lua_State *L ) { return SetGamePlayerControlData ( L, 703 ); }
int GetCharAnimStateObj ( lua_State *L ) { return GetGamePlayerControlData ( L, 703 ); }
int SetCharAnimStateAnimationSpeed ( lua_State *L ) { return SetGamePlayerControlData ( L, 704 ); }
int GetCharAnimStateAnimationSpeed ( lua_State *L ) { return GetGamePlayerControlData ( L, 704 ); }
int SetCharAnimStateE ( lua_State *L ) { return SetGamePlayerControlData ( L, 705 ); }
int GetCharAnimStateE ( lua_State *L ) { return GetGamePlayerControlData ( L, 705 ); }
int GetCsiStoodVault ( lua_State *L ) { return GetGamePlayerControlData ( L, 741 ); }
int GetCharSeqTrigger ( lua_State *L ) { return GetGamePlayerControlData ( L, 751 ); }
int GetEntityElementBankIndex ( lua_State *L ) { return GetGamePlayerControlData ( L, 761 ); }
int GetEntityElementObj ( lua_State *L ) { return GetGamePlayerControlData ( L, 762 ); }
int GetEntityElementRagdollified ( lua_State *L ) { return GetGamePlayerControlData ( L, 763 ); }
int GetEntityElementSpeedModulator ( lua_State *L ) { return GetGamePlayerControlData ( L, 764 ); }
int SetCharAnimControlsLeaping ( lua_State *L ) { return SetGamePlayerControlData ( L, 801 ); }
int GetCharAnimControlsLeaping ( lua_State *L ) { return GetGamePlayerControlData ( L, 801 ); }
int SetCharAnimControlsMoving ( lua_State *L ) { return SetGamePlayerControlData ( L, 802 ); }
int GetCharAnimControlsMoving ( lua_State *L ) { return GetGamePlayerControlData ( L, 802 ); }
int GetEntityProfileJumpModifier ( lua_State *L ) { return GetGamePlayerControlData ( L, 851 ); }
int GetEntityProfileStartOfAIAnim ( lua_State *L ) { return GetGamePlayerControlData ( L, 852 ); }
int GetEntityProfileJumpHold ( lua_State *L ) { return GetGamePlayerControlData ( L, 853 ); }
int GetEntityProfileJumpResume ( lua_State *L ) { return GetGamePlayerControlData ( L, 854 ); }

int GetEntityAnimStart ( lua_State *L ) { return GetGamePlayerControlData ( L, 1001 ); }
int GetEntityAnimFinish ( lua_State *L ) { return GetGamePlayerControlData ( L, 1002 ); }

int CombatControllerLaserGuidedHit(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iObjToHit = lua_tonumber(L, 1);
	float fX=0, fY=0, fZ=0;
	int iHitIt = GGVR_GetLaserGuidedHit (iObjToHit, &fX, &fY, &fZ);
	lua_pushnumber (L, iHitIt);
	lua_pushnumber (L, fX);
	lua_pushnumber (L, fY);
	lua_pushnumber (L, fZ);
	return 4;
}

int SetRotationYSlowly ( lua_State *L ) 
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	int iEntityID = lua_tonumber( L, 1 );
	float fDestAngle = lua_tonumber( L, 2 );
	float fSlowlyRate = lua_tonumber( L, 3 );
	int iStoreE = t.e; t.e = iEntityID;
	float fStoreV = t.v; t.v = fSlowlyRate;
	entity_lua_rotatetoanglecore ( fDestAngle, 0.0f );
	t.e = iStoreE;
	t.v = fStoreV;
	return 0; // pushes nothing (returning 1 handed the third argument back)
}


// Particle commands

int ParticlesGetFreeEmitter ( lua_State *L )
{
	lua = L;
	ravey_particles_get_free_emitter ( );
	lua_pushnumber ( L, t.tResult );
	return 1;
}

int ParticlesLoadImage(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;

	int iID = 0;
	if (n == 2) iID = lua_tonumber(L, 2);

	char pFileName[256];
	strcpy(pFileName, lua_tostring(L, 1));

	iID = ravey_particles_load_image(pFileName, iID);

	if (iID == -1) return 0;

	lua_pushnumber(L, iID);
	return 1;
}

int ParticlesLoadEffect(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;

	char pFileName[256];
	strcpy(pFileName, lua_tostring(L, 1));

	lua_pushnumber(L, ravey_particles_load_effect(pFileName, lua_tonumber(L, 2)));
	return 1;
}

int ParticlesSetFrames(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;

	ravey_particles_set_frames(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	return 0;
}

int ParticlesSetSpeed(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 7) return 0;

	ravey_particles_set_speed(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
		lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7));
	return 0;
}

int ParticlesSetGravity(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;

	ravey_particles_set_gravity(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3));
	return 0;
}

int ParticlesSetOffset(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 7) return 0;

	ravey_particles_set_offset(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
		lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7));
	return 0;
}

int ParticlesSetAngle(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;

	ravey_particles_set_angle(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	return 0;
}

int ParticlesSetRotation(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 8) return 0;

	ravey_particles_set_rotate(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
		lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8));
	return 0;
}

int ParticlesSetScale(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;

	ravey_particles_set_scale(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
		lua_tonumber(L, 5));
	return 0;
}

int ParticlesSetAlpha(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;

	ravey_particles_set_alpha(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
		lua_tonumber(L, 5));
	return 0;
}

int ParticlesSetLife( lua_State *L )
{
	lua = L;
	int n = lua_gettop( L );
	if ( n < 6 ) return 0;

	if ( n == 6 )
	{
		ravey_particles_set_life( lua_tonumber( L, 1 ), lua_tonumber( L, 2 ), lua_tonumber( L, 3 ), lua_tonumber( L, 4 ),
			                      lua_tonumber( L, 5 ), lua_tonumber( L, 6 ), RAVEY_PARTICLES_MAX_SPAWNED_AT_ONCE_BY_AN_EMITTER );
	}
	else
	{
		ravey_particles_set_life( lua_tonumber( L, 1 ), lua_tonumber( L, 2 ), lua_tonumber( L, 3 ), lua_tonumber( L, 4 ),
			                      lua_tonumber( L, 5 ), lua_tonumber( L, 6 ), lua_tonumber( L, 7 ) );
	}
	return 0;
}

int ParticlesSetWindVector(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;

	ravey_particles_set_wind_vector(lua_tonumber(L, 1), lua_tonumber(L, 2));
	return 0;
}

int ParticlesSetNoWind(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;

	ravey_particles_set_no_wind(lua_tonumber(L, 1));
	return 0;
}

int ParticlesSpawnParticle(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;

	if (n < 4)
	{
		ravey_particles_generate_particle(lua_tonumber(L, 1), 0, 0, 0);
	}
	else
	{
		ravey_particles_generate_particle(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	}
	return 0;
}

int ParticlesAddEmitterCore(lua_State *L, int iExtended)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (iExtended == 0)
	{
		if (n < 28) return 0;
	}
	else
	{
		if (n < 32) return 0;
	}

	// populate emitter data
	t.tResult = lua_tonumber(L, 1);
	float animationSpeed = lua_tonumber(L, 2);
	float startsOffRandomAngle = lua_tonumber(L, 4);
	float offsetMinX = lua_tonumber(L, 4);
	float offsetMinY = lua_tonumber(L, 5);
	float offsetMinZ = lua_tonumber(L, 6);
	float offsetMaxX = lua_tonumber(L, 7);
	float offsetMaxY = lua_tonumber(L, 8);
	float offsetMaxZ = lua_tonumber(L, 9);
	float scaleStartMin = lua_tonumber(L, 10);
	float scaleStartMax = lua_tonumber(L, 11);
	float scaleEndMin = lua_tonumber(L, 12);
	float scaleEndMax = lua_tonumber(L, 13);
	float movementSpeedMinX = lua_tonumber(L, 14);
	float movementSpeedMinY = lua_tonumber(L, 15);
	float movementSpeedMinZ = lua_tonumber(L, 16);
	float movementSpeedMaxX = lua_tonumber(L, 17);
	float movementSpeedMaxY = lua_tonumber(L, 18);
	float movementSpeedMaxZ = lua_tonumber(L, 19);
	float rotateSpeedMinZ = lua_tonumber(L, 20);
	float rotateSpeedMaxZ = lua_tonumber(L, 21);
	float lifeMin = lua_tonumber(L, 22);
	float lifeMax = lua_tonumber(L, 23);
	float alphaStartMin = lua_tonumber(L, 24);
	float alphaStartMax = lua_tonumber(L, 25);
	float alphaEndMin = lua_tonumber(L, 26);
	float alphaEndMax = lua_tonumber(L, 27);
	float frequency = lua_tonumber(L, 28);
	int entID = -1;
	int entLimbIndex = -1;
	int particleImage = RAVEY_PARTICLES_IMAGETYPE_LIGHTSMOKE + g.particlesimageoffset;
	int particleFrameCount = 64;
	if (iExtended == 1)
	{
		entID = lua_tonumber(L, 29);
		entLimbIndex = lua_tonumber(L, 30);
		int tCheckParticleImage = lua_tonumber(L, 31);
		if (tCheckParticleImage > 0)
		{
			particleImage = tCheckParticleImage + g.particlesimageoffset;
			particleFrameCount = lua_tonumber(L, 32);
		}
	}
	g.tEmitter.id = t.tResult;
	g.tEmitter.emitterLife = 0;
	if (entID == -1)
	{
		g.tEmitter.parentObject = t.aisystem.objectstartindex;
		g.tEmitter.parentLimb = 0;
	}
	else if (entID > 0)
	{
		g.tEmitter.parentObject = t.entityelement[entID].obj;
		g.tEmitter.parentLimb = entLimbIndex;
	}
	else
	{
		g.tEmitter.parentObject = 0;
	}
	g.tEmitter.isAnObjectEmitter = 0;
	g.tEmitter.startsOffRandomAngle = startsOffRandomAngle;
	g.tEmitter.offsetMinX = offsetMinX;
	g.tEmitter.offsetMinY = offsetMinY;
	g.tEmitter.offsetMinZ = offsetMinZ;
	g.tEmitter.offsetMaxX = offsetMaxX;
	g.tEmitter.offsetMaxY = offsetMaxY;
	g.tEmitter.offsetMaxZ = offsetMaxZ;
	g.tEmitter.scaleStartMin = scaleStartMin;
	g.tEmitter.scaleStartMax = scaleStartMax;
	g.tEmitter.scaleEndMin = scaleEndMin;
	g.tEmitter.scaleEndMax = scaleEndMax;
	g.tEmitter.movementSpeedMinX = movementSpeedMinX;
	g.tEmitter.movementSpeedMinY = movementSpeedMinY;
	g.tEmitter.movementSpeedMinZ = movementSpeedMinZ;
	g.tEmitter.movementSpeedMaxX = movementSpeedMaxX;
	g.tEmitter.movementSpeedMaxY = movementSpeedMaxY;
	g.tEmitter.movementSpeedMaxZ = movementSpeedMaxZ;
	g.tEmitter.rotateSpeedMinZ = rotateSpeedMinZ;
	g.tEmitter.rotateSpeedMaxZ = rotateSpeedMaxZ;
	g.tEmitter.startGravity = 0;
	g.tEmitter.endGravity = 0;
	g.tEmitter.lifeMin = lifeMin;
	g.tEmitter.lifeMax = lifeMax;
	g.tEmitter.alphaStartMin = alphaStartMin;
	g.tEmitter.alphaStartMax = alphaStartMax;
	g.tEmitter.alphaEndMin = alphaEndMin;
	g.tEmitter.alphaEndMax = alphaEndMax;
	g.tEmitter.frequency = frequency;

	// fixed animation for smoke
	g.tEmitter.imageNumber = particleImage;//RAVEY_PARTICLES_IMAGETYPE_LIGHTSMOKE + g.particlesimageoffset;
	g.tEmitter.isAnimated = 1;
	g.tEmitter.frameCount = particleFrameCount;

	if (animationSpeed > 0)
	{
		g.tEmitter.animationSpeed = animationSpeed;
		g.tEmitter.isLooping = 1;
		g.tEmitter.startFrame = 0;
		g.tEmitter.endFrame = g.tEmitter.frameCount - 1;
	}
	else
	{
		g.tEmitter.animationSpeed = 0;
		g.tEmitter.isLooping = 0;
	}

	// create emitter
	ravey_particles_add_emitter();

	return 0;
}

int ParticlesAddEmitter( lua_State *L )
{
	return ParticlesAddEmitterCore ( L, 0 );
}

int ParticlesAddEmitterEx( lua_State *L )
{
	return ParticlesAddEmitterCore ( L, 1 );
}

int ParticlesDeleteEmitter( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	t.tRaveyParticlesEmitterID = lua_tonumber(L, 1);
	ravey_particles_delete_emitter ( );
	return 0;
}

// New Particle Effects System

int EffectStart(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int e = lua_tonumber(L, 1);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_Show_At_Start = 1;
	return 0;
}
int EffectStop(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int e = lua_tonumber(L, 1);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_Show_At_Start = 0;
	return 0;
}
int EffectSetLocalPosition(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int e = lua_tonumber(L, 1);
	float x = lua_tonumber(L, 2);
	float y = lua_tonumber(L, 3);
	float z = lua_tonumber(L, 4);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_Offset_Used = true;
	t.entityelement[e].eleprof.newparticle.bParticle_Offset_X = x;
	t.entityelement[e].eleprof.newparticle.bParticle_Offset_Y = y;
	t.entityelement[e].eleprof.newparticle.bParticle_Offset_Z = z;
	return 0;
}
int EffectSetLocalRotation(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int e = lua_tonumber(L, 1);
	float x = lua_tonumber(L, 2);
	float y = lua_tonumber(L, 3);
	float z = lua_tonumber(L, 4);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_LocalRot_Used = true;
	t.entityelement[e].eleprof.newparticle.bParticle_LocalRot_X = x;
	t.entityelement[e].eleprof.newparticle.bParticle_LocalRot_Y = y;
	t.entityelement[e].eleprof.newparticle.bParticle_LocalRot_Z = z;
	return 0;
}
int EffectSetSpeed(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tonumber(L, 1);
	float speed = lua_tonumber(L, 2) / 100.0f;
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_SpeedChange = true;
	t.entityelement[e].eleprof.newparticle.fParticle_Speed = speed;
	return 0;
}
int EffectSetOpacity(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tonumber(L, 1);
	float opacity = lua_tonumber(L, 2) / 100.0f;
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_OpacityChange = true;
	t.entityelement[e].eleprof.newparticle.fParticle_Opacity = opacity;
	return 0;
}
//EffectGetOpacity(e) - opacity in percent, as EffectSetOpacity takes it
int EffectGetOpacity(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int e = lua_tonumber(L, 1);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	newparticletype* pParticle = &t.entityelement[e].eleprof.newparticle;
	float opacity = 1.0f;
	if (pParticle->bParticle_OpacityChange == true)
		opacity = pParticle->fParticle_Opacity;
	else if (pParticle->bWPE == false && pParticle->emitterid != -1)
		opacity = GPUParticles::gpup_getParticleOpacity(pParticle->emitterid);
	lua_pushnumber(L, opacity * 100.0f);
	return 1;
}
int EffectSetParticleSize(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tonumber(L, 1);
	float opacity = lua_tonumber(L, 2) / 100.0f;
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_SizeChange = true;
	t.entityelement[e].eleprof.newparticle.bParticle_Size = opacity;
	return 0;
}
int EffectSetBurstMode(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tonumber(L, 1);
	int automode = lua_tonumber(L, 2);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_Looping_Animation = 1 - automode;
	return 0;
}
int EffectFireBurst(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int e = lua_tonumber(L, 1);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_Fire = true;
	return 0;
}
int EffectSetFloorReflection(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	int e = lua_tonumber(L, 1);
	int active = lua_tonumber(L, 2);
	float height = lua_tonumber(L, 3);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.iParticle_Floor_Active = 1 + active;
	t.entityelement[e].eleprof.newparticle.fParticle_Floor_Height = height;
	return 0;
}
int EffectSetBounciness(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tonumber(L, 1);
	float bounciness = lua_tonumber(L, 2) / 100.0f;
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.fParticle_BouncinessChange = true;
	t.entityelement[e].eleprof.newparticle.fParticle_Bounciness = bounciness;
	return 0;
}
int EffectSetColor(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int e = lua_tonumber(L, 1);
	float r = lua_tonumber(L, 2);
	float g = lua_tonumber(L, 3);
	float b = lua_tonumber(L, 4);
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_ColorChange = true;
	t.entityelement[e].eleprof.newparticle.fParticle_R = r;
	t.entityelement[e].eleprof.newparticle.fParticle_G = g;
	t.entityelement[e].eleprof.newparticle.fParticle_B = b;
	return 0;
}
int EffectSetLifespan(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tonumber(L, 1);
	float lifespan = lua_tonumber(L, 2) * 10.0f;
	if (!LuaEntityIDValid(L, e, 1)) return 0;
	t.entityelement[e].eleprof.newparticle.bParticle_LifespanChange = true;
	t.entityelement[e].eleprof.newparticle.fParticle_Lifespan = lifespan;
	return 0;
}

#ifdef WICKEDPARTICLESYSTEM
std::vector<uint32_t> vWickedEmitterEffects;
void DeleteEmitterEffects(uint32_t root)
{
	Scene& scene = wiScene::GetScene();

	std::vector<uint32_t> vEntityDelete;
	for (int a = 0; a < scene.emitters.GetCount(); a++)
	{
		Entity emitter = scene.emitters.GetEntity(a);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier)
		{
			if (hier->parentID == root)
			{
				vEntityDelete.push_back(emitter);
			}
		}
	}
	vEntityDelete.push_back(root);

	for (int i = 0; i < vEntityDelete.size(); i++)
	{
		scene.Entity_Remove(vEntityDelete[i]);
	}
	vEntityDelete.clear();
}
void CleanUpEmitterEffects(void)
{
	Scene& scene = wiScene::GetScene();

	std::vector<uint32_t> vEntityDelete;
	for (int i = 0; i < vWickedEmitterEffects.size(); i++)
	{
		uint32_t root = vWickedEmitterEffects[i];
		for (int a = 0; a < scene.emitters.GetCount(); a++)
		{
			Entity emitter = scene.emitters.GetEntity(a);
			HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
			if (hier)
			{
				if (hier->parentID == root)
				{
					vEntityDelete.push_back(emitter);
				}
			}
		}
		vEntityDelete.push_back(root);
	}

	for (int i = 0; i < vEntityDelete.size(); i++)
	{
		scene.Entity_Remove(vEntityDelete[i]);
	}
	vEntityDelete.clear();
	vWickedEmitterEffects.clear();
}

//PE: WParticleEffectPosition("FileName") - Return EffectID
int WParticleEffectLoad(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;

	// disable wicked particles (for testing/etc)
	extern int g_iDisableWParticleSystem;
	if (g_iDisableWParticleSystem == 1)
	{
		lua_pushnumber(L, 0);
		return 1;
	}
	
	Scene& scene = wiScene::GetScene();

	char pFileName[MAX_PATH];
	strcpy(pFileName, lua_tostring(L, 1));

	uint32_t root = 0;
	uint32_t count_before = scene.emitters.GetCount();

	cstr pOldDir = GetDir();

	char path[MAX_PATH];
	strcpy(path, pFileName);
	GG_GetRealPath(path, 0);

	WickedCall_LoadWiScene(path, false, NULL, NULL);
	uint32_t count_after = scene.emitters.GetCount();
	if (count_before != count_after)
	{
		Entity emitter = scene.emitters.GetEntity(scene.emitters.GetCount() - 1);
		if (scene.emitters.GetCount() > 0)
		{
			HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
			if (hier)
			{
				root = hier->parentID;
			}
		}
		wiEmittedParticle* ec = scene.emitters.GetComponent(emitter);
		if (ec)
		{
			ec->Restart();
			ec->SetVisible(true);
		}
	}
	if (root != 0)
		vWickedEmitterEffects.push_back(root);
	lua_pushnumber(L, root);
	return 1;

}
//PE: WParticleEffectPosition(EffectID,x,y,z)
int WParticleEffectPosition(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;

	Entity root = lua_tonumber(L, 1);
	float fX = lua_tonumber(L, 2);
	float fY = lua_tonumber(L, 3);
	float fZ = lua_tonumber(L, 4);
	float fXa = 0;
	float fYa = 0;
	float fZa = 0;
	bool bRot = false;
	if (n >= 7)
	{
		fXa = lua_tonumber(L, 5);
		fYa = lua_tonumber(L, 6);
		fZa = lua_tonumber(L, 7);
		bRot = true;
	}
	Scene& scene = wiScene::GetScene();
	TransformComponent* root_tranform = scene.transforms.GetComponent(root);
	if (root_tranform)
	{
		float normalizedDegreesX = fmod(fXa, 360.0f);
		if (fXa < 0)
			fXa += 360.0f;
		float rotationRadiansX = fXa * (XM_PI / 180.0f); //PE: to radians
		float normalizedDegreesY = fmod(fYa, 360.0f);
		if (fYa < 0)
			fYa += 360.0f;
		float rotationRadiansY = fYa * (XM_PI / 180.0f); //PE: to radians
		float normalizedDegreesZ = fmod(fZa, 360.0f);
		if (fZa < 0)
			fZa += 360.0f;
		float rotationRadiansZ = fZa * (XM_PI / 180.0f); //PE: to radians

		root_tranform->ClearTransform();
		root_tranform->Translate(XMFLOAT3(fX, fY, fZ));
		XMFLOAT3 rot = { rotationRadiansX ,rotationRadiansY ,rotationRadiansZ }; //PE: 0 - XM_2PI
		if(bRot)
			root_tranform->RotateRollPitchYaw(rot);
		root_tranform->UpdateTransform();
	}

	//for (int i = 0; i < scene.emitters.GetCount(); i++)
	//{
	//	Entity emitter = scene.emitters.GetEntity(i);
	//	HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
	//	if (hier)
	//	{
	//		if (hier->parentID == root)
	//		{
	//		}
	//	}
	//}

	return 0;
}
int WParticleEffectVisible(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;

	Entity root = lua_tonumber(L, 1);
	bool bVisible = lua_tonumber(L, 2);

	Scene& scene = wiScene::GetScene();

	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier)
		{
			if (hier->parentID == root)
			{
				wiEmittedParticle* ec = scene.emitters.GetComponent(emitter);
				if (ec)
				{
					ec->SetVisible(bVisible);
				}
			}
		}
	}

	return 0;
}

//WParticleEffectAction(EffectID,Action) - Action = 1 Burst all. 2 = Pause. - 3 = Resume. - 4 = Restart
int WParticleEffectAction(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	Entity root = lua_tonumber(L, 1);
	int iAction = lua_tonumber(L, 2);
	WickedCall_PerformEmitterAction(iAction, root);
	return 0;
}

//WParticleEffectSetKillBox(EffectID,Slot,MinX,MinY,MinZ,MaxX,MaxY,MaxZ) - Slot 1-8. Particles of the effect inside the world box are removed. Without the box the slot is cleared.
int WParticleEffectSetKillBox(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	Entity root = lua_tonumber(L, 1);
	int iSlot = lua_tonumber(L, 2) - 1;
	if (iSlot < 0 || iSlot >= (int)EMITTER_KILLBOX_COUNT) return 0;
	XMFLOAT4 vMin = XMFLOAT4(FLT_MAX, FLT_MAX, FLT_MAX, 0);
	XMFLOAT4 vMax = XMFLOAT4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0);
	if (n >= 8)
	{
		float fX1 = lua_tonumber(L, 3);
		float fY1 = lua_tonumber(L, 4);
		float fZ1 = lua_tonumber(L, 5);
		float fX2 = lua_tonumber(L, 6);
		float fY2 = lua_tonumber(L, 7);
		float fZ2 = lua_tonumber(L, 8);
		vMin = XMFLOAT4(min(fX1, fX2), min(fY1, fY2), min(fZ1, fZ2), 0);
		vMax = XMFLOAT4(max(fX1, fX2), max(fY1, fY2), max(fZ1, fZ2), 0);
	}

	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier && hier->parentID == root)
		{
			wiEmittedParticle& ec = scene.emitters[i];
			ec.killbox_min[iSlot] = vMin;
			ec.killbox_max[iSlot] = vMax;
			if (ec.killbox_count < (uint32_t)iSlot + 1) ec.killbox_count = (uint32_t)iSlot + 1;
		}
	}
	return 0;
}

//WParticleEffectClearKillBoxes(EffectID)
int WParticleEffectClearKillBoxes(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	Entity root = lua_tonumber(L, 1);

	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier && hier->parentID == root)
		{
			scene.emitters[i].killbox_count = 0;
		}
	}
	return 0;
}

//WParticleEffectSetOpacity(EffectID,Opacity) - Opacity in percent of the opacity the effect was made with (100 = as made).
int WParticleEffectSetOpacity(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	Entity root = lua_tonumber(L, 1);
	float fOpacity = max(0.0f, (float)lua_tonumber(L, 2) / 100.0f);

	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier && hier->parentID == root)
		{
			scene.emitters[i].opacity_scale = fOpacity;
		}
	}
	return 0;
}

//WParticleEffectGetOpacity(EffectID) - Returns the opacity in percent set by WParticleEffectSetOpacity (100 by default), nil if the effect has no emitters.
int WParticleEffectGetOpacity(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	Entity root = lua_tonumber(L, 1);

	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier && hier->parentID == root)
		{
			lua_pushnumber(L, scene.emitters[i].opacity_scale * 100.0f);
			return 1;
		}
	}
	return 0;
}

//WParticleEffectSetSize(EffectID,Size) - Size in percent of the particle size the effect was made with (100 = as made).
int WParticleEffectSetSize(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	Entity root = lua_tonumber(L, 1);
	float fSize = max(0.0f, (float)lua_tonumber(L, 2) / 100.0f);

	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier && hier->parentID == root)
		{
			scene.emitters[i].size_scale = fSize;
		}
	}
	return 0;
}

//WParticleEffectGetSize(EffectID) - Returns the size in percent set by WParticleEffectSetSize (100 by default), nil if the effect has no emitters.
int WParticleEffectGetSize(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	Entity root = lua_tonumber(L, 1);

	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier && hier->parentID == root)
		{
			lua_pushnumber(L, scene.emitters[i].size_scale * 100.0f);
			return 1;
		}
	}
	return 0;
}

//WParticleEffectSetColor(EffectID,Red,Green,Blue) - 0 to 255 each, multiplying the colour the effect was made with (255,255,255 = as made).
int WParticleEffectSetColor(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	Entity root = lua_tonumber(L, 1);
	XMFLOAT3 tint = XMFLOAT3(max(0.0f, (float)lua_tonumber(L, 2) / 255.0f), max(0.0f, (float)lua_tonumber(L, 3) / 255.0f), max(0.0f, (float)lua_tonumber(L, 4) / 255.0f));

	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier && hier->parentID == root)
		{
			scene.emitters[i].color_tint = tint;
		}
	}
	return 0;
}

//WParticleEffectGetColor(EffectID) - Returns Red,Green,Blue set by WParticleEffectSetColor (255,255,255 by default), nothing if the effect has no emitters.
int WParticleEffectGetColor(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	Entity root = lua_tonumber(L, 1);

	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier && hier->parentID == root)
		{
			lua_pushnumber(L, scene.emitters[i].color_tint.x * 255.0f);
			lua_pushnumber(L, scene.emitters[i].color_tint.y * 255.0f);
			lua_pushnumber(L, scene.emitters[i].color_tint.z * 255.0f);
			return 3;
		}
	}
	return 0;
}

//WParticleEffectGetBounds(EffectID) - Returns MinX,MinY,MinZ,MaxX,MaxY,MaxZ of the world area the effect's particles are born in, as of the last frame (particles then move by their speed and gravity). Nothing if the effect has no emitters.
int WParticleEffectGetBounds(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	Entity root = lua_tonumber(L, 1);

	Scene& scene = wiScene::GetScene();
	AABB bounds;
	bool bFound = false;
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		TransformComponent* transform = scene.transforms.GetComponent(emitter);
		if (hier && transform && hier->parentID == root)
		{
			const wiEmittedParticle& ec = scene.emitters[i];
			const MeshComponent* mesh = scene.meshes.GetComponent(ec.meshID);
			AABB area;
			if (mesh)
			{
				// emits from the mesh surface
				area = mesh->aabb.transform(transform->world);
			}
			else if (ec.IsVolumeEnabled())
			{
				// emits inside the unit cube under the emitter transform (emittedparticle_emitCS EMITTER_VOLUME)
				area = AABB(XMFLOAT3(-1, -1, -1), XMFLOAT3(1, 1, 1)).transform(transform->world);
			}
			else
			{
				// emits at the emitter origin, spread on X and Z by random_position_scale
				XMFLOAT3 pos = transform->GetPosition();
				float fHalf = fabs(ec.random_position_scale) * 0.5f;
				area = AABB(XMFLOAT3(pos.x - fHalf, pos.y, pos.z - fHalf), XMFLOAT3(pos.x + fHalf, pos.y, pos.z + fHalf));
			}
			// startpos adds a swing of up to x and z, and y/2
			area._min.x -= fabs(ec.startpos.x);
			area._max.x += fabs(ec.startpos.x);
			area._min.y -= fabs(ec.startpos.y) * 0.5f;
			area._max.y += fabs(ec.startpos.y) * 0.5f;
			area._min.z -= fabs(ec.startpos.z);
			area._max.z += fabs(ec.startpos.z);
			bounds = AABB::Merge(bounds, area);
			bFound = true;
		}
	}
	if (!bFound) return 0;
	lua_pushnumber(L, bounds._min.x);
	lua_pushnumber(L, bounds._min.y);
	lua_pushnumber(L, bounds._min.z);
	lua_pushnumber(L, bounds._max.x);
	lua_pushnumber(L, bounds._max.y);
	lua_pushnumber(L, bounds._max.z);
	return 6;
}
//disableindoor
// rotate
// Stop
// copy lua code from app.
// add emitter with all settings.
// follow mesh.
// follow bone.

#endif

//PE: Missing command for position sound if different then entity position.
int entity_lua_positionsound(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;
	int e = lua_tonumber(L, 1);
	int v = lua_tonumber(L, 2);
	float fX = lua_tonumber(L, 3);
	float fY = lua_tonumber(L, 4);
	float fZ = lua_tonumber(L, 5);
	int entity_lua_sound_convertVtoTSND(int te, int tv);
	int snd = entity_lua_sound_convertVtoTSND(e, v);
	if (snd > 0)
	{
		PositionSound(snd, fX, fY, fZ);
	}
	return 0;
}

//PE: Tracers commands.
int LoadTracerImage(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	char FileName[MAX_PATH];
	strcpy(FileName, lua_tostring(L, 1));

	int iImageID = lua_tonumber(L, 2);
	if (iImageID < 0 || iImageID >= 100)
		iImageID = 0;

	iImageID += 400; //PE: LUA offset

	Tracers::LoadTracerImage(FileName, iImageID);
	return 0;
}

int AddTracer(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 16) return 0;

	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fXt = lua_tonumber(L, 4);
	float fYt = lua_tonumber(L, 5);
	float fZt = lua_tonumber(L, 6);

	XMFLOAT3 tracer_from, tracer_hit;

	tracer_from.x = fX;
	tracer_from.y = fY;
	tracer_from.z = fZ;
	tracer_hit.x = fXt;
	tracer_hit.y = fYt;
	tracer_hit.z = fZt;


	float lifetime = lua_tonumber(L, 7);
	float colR = lua_tonumber(L, 8) / 255.0f;
	float colG = lua_tonumber(L, 9) / 255.0f;
	float colB = lua_tonumber(L, 10) / 255.0f;
	float glow = lua_tonumber(L, 11);
	float scrollV = lua_tonumber(L, 12);
	float scaleV = lua_tonumber(L, 13);
	float width = lua_tonumber(L, 14);
	float maxlength = lua_tonumber(L, 15);
	int iImageID = lua_tonumber(L, 16);
	iImageID += 400; //PE: LUA offset

	Tracers::AddTracer(
		tracer_from,
		tracer_hit,
		lifetime, // Lifetime
		XMFLOAT4(colR, colG, colB, 1), // Color
		glow, // 5.0f, // Glow
		scrollV, // Scroll
		scaleV, // scaleV
		width, // width
		maxlength, // max length
		iImageID // TextureID
	);
	return 0;
}
// Misc Commands

int GetBulletHit(lua_State* L)
{
	if (t.tdamagesource == 1)
	{
		lua_pushnumber(L, g.decalx);
		lua_pushnumber(L, g.decaly);
		lua_pushnumber(L, g.decalz);
		lua_pushnumber(L, t.tttriggerdecalimpact);
		lua_pushnumber(L, t.playercontrol.thirdperson.charactere);
		t.tdamagesource = 0;
		return 5;
	}
	return 0;
}

// Image commands

int GetFreeLUAImageID ( void )
{
	for ( int c = g.LUAImageoffset ; c <= g.LUAImageoffsetMax ; c++ )
	{
		if ( ImageExist ( c ) == 0 )
			return c;
	}

	return 0;
}

int GetFreeLUASpriteID ( void )
{
	for ( int c = g.LUASpriteoffset ; c <= g.LUASpriteoffsetMax ; c++ )
	{
		if ( SpriteExist ( c ) == 0 )
			return c;
	}
	return 0;
}

void FreeLUASpritesAndImages ( void )
{
	for ( int c = g.LUASpriteoffset ; c <= g.LUASpriteoffsetMax ; c++ )
	{
		if ( SpriteExist ( c ) == 1 )
			DeleteSprite ( c );
	}
	//PE: All lua loadimage files are loaded in legacy mode , so also need it for textures to be deleted.
	image_setlegacyimageloading(true);
	for ( int c = g.LUAImageoffset ; c <= g.LUAImageoffsetMax ; c++ )
	{
		if ( ImageExist ( c ) == 1 )
			DeleteImage ( c );
	}
	image_setlegacyimageloading(false);
}

void HideOrShowLUASprites ( bool hide )
{
	for ( int c = g.LUASpriteoffset ; c <= g.LUASpriteoffsetMax ; c++ )
	{
		if ( SpriteExist ( c ) == 1 )
		{
			if ( hide ) 
				HideSprite ( c );
			else
				ShowSprite ( c );
		}
	}
}

int SetFlashLight ( lua_State *L )
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 1 )
		return 0;

	int mode = lua_tointeger(L, 1);

	if ( mode == 1 )
		t.playerlight.flashlightcontrol_f = 1.0f;
	else
		t.playerlight.flashlightcontrol_f = 0.0f;

		return 0;
}

int SetFlashLightPosition(lua_State* L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if (n < 3)
		return 0;
	if (n >= 3)
	{
		t.playerlight.flashlightcontrol_forward = lua_tonumber(L, 1);
		t.playerlight.flashlightcontrol_right = lua_tonumber(L, 2);
		t.playerlight.flashlightcontrol_down = lua_tonumber(L, 3);
	}
	if (n == 6)
	{
		t.playerlight.flashlightcontrol_anglex = lua_tonumber(L, 4);
		t.playerlight.flashlightcontrol_angley = lua_tonumber(L, 5);
		t.playerlight.flashlightcontrol_anglez = lua_tonumber(L, 6);
	}
	
	return 0;
}
int GetFlashLightPosition(lua_State* L)
{
	lua_pushnumber(L, t.playerlight.flashlightcontrol_forward);
	lua_pushnumber(L, t.playerlight.flashlightcontrol_right);
	lua_pushnumber(L, t.playerlight.flashlightcontrol_down);
	return 3;
}


int SetPlayerRun ( lua_State *L )
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 1 )
		return 0;

	int mode = lua_tointeger(L, 1);

	t.playercontrol.canrun = mode;

	return 0;
}

int SetFont ( lua_State *L )
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 2 )
		return 0;

	char fileName[MAX_PATH];
	char path[MAX_PATH];
	char finalPath[MAX_PATH];
	char fpspath[MAX_PATH];

	strcpy ( fpspath , g.fpscrootdir_s.Get() );
	strcat ( fpspath , "\\Files\\" );

	strcpy ( finalPath , "" );

	strcpy ( fileName , lua_tostring(L,1) );
	strcpy ( path , fpspath );
	strcat ( path , "fontbank\\FPSCR-Font-" );
	strcat ( path , fileName ); 
	if ( FileExist ( path ) == 1 )
		strcpy ( finalPath , path );

	strcpy ( fileName , lua_tostring(L,1) );
	strcpy ( path , fpspath );
	strcat ( path , "fontbank\\FPSCR-" );
	strcat ( path , fileName ); 
	if ( FileExist ( path ) == 1 )
		strcpy ( finalPath , path );

	strcpy ( fileName , lua_tostring(L,1) );
	strcpy ( path , fpspath );
	strcat ( path , "fontbank\\" );
	strcat ( path , fileName ); 
	if ( FileExist ( path ) == 1 )
		strcpy ( finalPath , path );

	if ( strcmp ( finalPath , "" ) == 0 ) return 0;

	if (finalPath[strlen(finalPath) - 4] == '.')
		// this is a c style string terminator, so alternative to fix custom fonts for Cogwheel
		//finalPath[strlen(finalPath)-4] = '\0';
	{
		finalPath[strlen(finalPath) - 4] = 0;
	}

	changebitmapfont(finalPath, lua_tointeger(L, 2));

	return 0;
}

// GetTextWidth(size,text) and GetTextHeight(size) measure Text() output in the same screen percent units Text() is positioned in
int GetTextWidth ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iSize = lua_tonumber(L, 1);
	const char* pText = lua_tostring(L, 2);
	float fWidth = 0.0f;
	if ( pText && GetDisplayWidth() > 0 ) fWidth = (getbitmapfontwidth((char*)pText, iSize) * 100.0f) / GetDisplayWidth();
	lua_pushnumber ( L, fWidth );
	return 1;
}

int GetTextHeight ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iSize = lua_tonumber(L, 1);
	float fHeight = 0.0f;
	if ( GetDisplayHeight() > 0 ) fHeight = (getbitmapfontheight(iSize) * 100.0f) / GetDisplayHeight();
	lua_pushnumber ( L, fHeight );
	return 1;
}

int SetOcclusion ( lua_State *L )
{

	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 1 )
		return 0;

	t.slidersmenuvalue[t.slidersmenunames.graphicoptions][5].value = lua_tointeger(L, 1);
	CPU3DSetPolyCount ( t.slidersmenuvalue[t.slidersmenunames.graphicoptions][5].value );

	return 0;
}

int SetFlashLightKeyEnabled ( lua_State *L )
{

	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 1 )
		return 0;

	int value = lua_tointeger(L, 1);

	if ( value != 0 && value != 1 ) return 0;

	g.flashLightKeyEnabled = value;

	return 0;
}

int SetPlayerWeapons ( lua_State *L )
{

	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 1 )
		return 0;

	int mode = lua_tointeger(L, 1);
	
	if ( mode == 0 )
	{
		g.noPlayerGuns = true;
		g.remembergunid = g.autoloadgun;
		if ( g.autoloadgun != 0 ) 
		{ 
			g.autoloadgun=0 ;
			gun_change ( );
		}
		physics_no_gun_zoom ( );
	}
	else
	{	
		g.noPlayerGuns = false;
		
		if ( g.remembergunid != 0 ) 
		{ 
			g.autoloadgun = g.remembergunid;			
			gun_change ( );
			physics_no_gun_zoom ( );
			g.remembergunid = 0;
		}		
	}

	return 0;
}

int FirePlayerWeapon(lua_State *L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;

	int firingmode = lua_tointeger(L, 1);
	//t.player[1].state.firingmode = firingmode; // 121215 - seems when in game, this gets ignored so no gunshoot happens..
	if ( firingmode == 1 )
	{
		if ( t.gunmode < 101 )
			t.gunmode = 101;
	}
	if ( firingmode >= 2 )
	{
		t.player[1].state.firingmode = firingmode;
	}

	return 0;
}

// AddBulletHole(x, y, z, nx, ny, nz [, material [, e]]): a bullet hole where a script's shot hit, as the player's gunfire
// leaves one. material is an entity profile's materialindex (1 stone, 2 metal, 3 wood, 4 glass; default 1; 0 leaves
// none). With e, the hole obeys the same rules as gunfire: only on a static entity or one that allows bullet holes, and
// it goes when that entity moves, hides or is removed. Returns 1 if a hole was requested, 0 if a rule refused it
int AddBulletHole ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	float fNX = lua_tonumber(L, 4), fNY = lua_tonumber(L, 5), fNZ = lua_tonumber(L, 6);
	float fLength = sqrtf ( fNX*fNX + fNY*fNY + fNZ*fNZ );
	int iMaterial = 1;
	if ( n >= 7 ) iMaterial = max ( 0, min ( 15, (int)lua_tointeger(L, 7) ) );
	int e = 0;
	if ( n >= 8 ) e = lua_tointeger(L, 8);
	bool bAllowed = fLength > 0.0001f && iMaterial > 0 && ObjectExist ( g.bulletholesobject ) == 1;
	if ( bAllowed && e != 0 )
	{
		bAllowed = LuaEntityIDValid ( L, e, 1 ) && entity_allowsbulletholes ( e );
	}
	if ( bAllowed ) bulletholes_add ( iMaterial, lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), fNX / fLength, fNY / fLength, fNZ / fLength, e );
	lua_pushinteger(L, bAllowed ? 1 : 0);
	return 1;
}

// BulletRay(x1, y1, z1, x2, y2, z2 [, ignoree [, holes [, terrainmaterial]]]): a script weapon's shot, detected as the player's
// bullets are (entity_scriptbulletray). ignoree is the shooter's own entity (0 for none). holes 1 (default) leaves a bullet hole
// under the player's rules, 0 only detects. terrainmaterial 0 (default) leaves no hole on terrain, as the player's guns; 1 to 15
// uses that material. Returns hit (0 none, 1 terrain, 2 object, 5 tree), e (0 if not an entity), x, y, z, nx, ny, nz, material, hole (1/0)
int BulletRay ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	int iIgnoreObj = 0;
	if ( n >= 7 )
	{
		int iIgnoreE = lua_tointeger(L, 7);
		if ( iIgnoreE > 0 && LuaEntityIDValid ( L, iIgnoreE, 1 ) ) iIgnoreObj = t.entityelement[iIgnoreE].obj;
	}
	bool bLeaveHole = true;
	if ( n >= 8 ) bLeaveHole = lua_tointeger(L, 8) != 0;
	int iTerrainMaterial = 0;
	if ( n >= 9 ) iTerrainMaterial = max ( 0, min ( 15, (int)lua_tointeger(L, 9) ) );
	sScriptBulletRayHit hit;
	int iHit = entity_scriptbulletray ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tonumber(L, 6), iIgnoreObj, bLeaveHole, iTerrainMaterial, &hit );
	lua_pushinteger(L, iHit);
	lua_pushinteger(L, hit.e);
	lua_pushnumber(L, hit.fX);
	lua_pushnumber(L, hit.fY);
	lua_pushnumber(L, hit.fZ);
	lua_pushnumber(L, hit.fNX);
	lua_pushnumber(L, hit.fNY);
	lua_pushnumber(L, hit.fNZ);
	lua_pushinteger(L, hit.iMaterial);
	lua_pushinteger(L, hit.bHole ? 1 : 0);
	return 10;
}

// GetPlayerHitSeq(): the newest entry in the player's hit record, 0 if none this level. Entries never repeat a number
int GetPlayerHitSeq ( lua_State *L )
{
	lua = L;
	lua_pushinteger(L, playerhit_getseq());
	return 1;
}

// GetPlayerHit(seq): one entry of the player's hit record (G-Entity.cpp playerhit_*), nothing if not written yet or
// older than the 128 kept. Returns hit (0 none, 1 terrain, 2 entity, 3 other object, 4 passed through, 5 tree), e, x, y, z,
// nx, ny, nz, material, hole (the same first ten as BulletRay), then limb, damage, health before, health after,
// killed, kind (1 round, 2 a later pellet of the pull, 3 melee, 4 a player projectile's hit or blast), shot, gunid,
// and ox, oy, oz where the ray started. Reading does not remove the entry
int GetPlayerHit ( lua_State *L )
{
	lua = L;
	if ( LUA_GETTOP(L) < 1 ) return 0;
	sPlayerHit* pHit = playerhit_get ( (int)lua_tointeger(L, 1) );
	if ( !pHit ) return 0;
	lua_pushinteger(L, pHit->iHit);
	lua_pushinteger(L, pHit->e);
	lua_pushnumber(L, pHit->fX);
	lua_pushnumber(L, pHit->fY);
	lua_pushnumber(L, pHit->fZ);
	lua_pushnumber(L, pHit->fNX);
	lua_pushnumber(L, pHit->fNY);
	lua_pushnumber(L, pHit->fNZ);
	lua_pushinteger(L, pHit->iMaterial);
	lua_pushinteger(L, pHit->iHole);
	lua_pushinteger(L, pHit->iLimb);
	lua_pushinteger(L, pHit->iDamage);
	lua_pushinteger(L, pHit->iHealthBefore);
	lua_pushinteger(L, pHit->iHealthAfter);
	lua_pushinteger(L, pHit->iKilled);
	lua_pushinteger(L, pHit->iKind);
	lua_pushinteger(L, pHit->iShot);
	lua_pushinteger(L, pHit->iGunID);
	lua_pushnumber(L, pHit->fOX);
	lua_pushnumber(L, pHit->fOY);
	lua_pushnumber(L, pHit->fOZ);
	return 21;
}

// SetEntityInstanceEmissive(e, r, g, b [, strength]): per instance multiplier, 0 to 1, on the entity's emissive colour
// and strength as authored (1,1,1,1 = unchanged, strength 0 = no glow). Unlike the material setters it changes this
// instance only, not every instance of the same object
int SetEntityInstanceEmissive ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int e = lua_tointeger(L, 1);
	if ( !LuaEntityIDValid ( L, e, 1 ) ) return 0;
	float fStrength = 1.0f;
	if ( n >= 5 ) fStrength = lua_tonumber(L, 5);
	int iObj = t.entityelement[e].obj;
	if ( iObj > 0 && ObjectExist ( iObj ) == 1 )
		WickedCall_SetObjectEmissiveTint ( GetObjectData ( iObj ), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), fStrength );
	return 0;
}

int GetEntityInstanceEmissive ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	float fR = 0, fG = 0, fB = 0, fStrength = 0;
	if ( LuaEntityIDValid ( L, e, 1 ) )
	{
		int iObj = t.entityelement[e].obj;
		if ( iObj > 0 && ObjectExist ( iObj ) == 1 )
			WickedCall_GetObjectEmissiveTint ( GetObjectData ( iObj ), &fR, &fG, &fB, &fStrength );
	}
	lua_pushnumber ( L, fR );
	lua_pushnumber ( L, fG );
	lua_pushnumber ( L, fB );
	lua_pushnumber ( L, fStrength );
	return 4;
}

// SetEntityInstanceTint(e, r, g, b): per instance multiplier, 0 to 1, on the entity's base colour (1,1,1 = unchanged), e.g.
// to darken a wreck or colour one vehicle. Unlike the material setters it changes this instance only
int SetEntityInstanceTint ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int e = lua_tointeger(L, 1);
	if ( !LuaEntityIDValid ( L, e, 1 ) ) return 0;
	int iObj = t.entityelement[e].obj;
	if ( iObj > 0 && ObjectExist ( iObj ) == 1 )
		WickedCall_SetObjectColorTint ( GetObjectData ( iObj ), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}

int GetEntityInstanceTint ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	float fR = 1, fG = 1, fB = 1;
	if ( LuaEntityIDValid ( L, e, 1 ) )
	{
		int iObj = t.entityelement[e].obj;
		if ( iObj > 0 && ObjectExist ( iObj ) == 1 )
			WickedCall_GetObjectColorTint ( GetObjectData ( iObj ), &fR, &fG, &fB );
	}
	lua_pushnumber ( L, fR );
	lua_pushnumber ( L, fG );
	lua_pushnumber ( L, fB );
	return 3;
}

// GetEntityImmunity(e): 0 when damage can take the entity's health to 0; otherwise its health stays at 1 or more and
// SetEntityHealth(e, 0) does nothing: -1 held by SetEntityHealth(e, -99999) until SetEntityHealthSilent(e, -1) or (e, 0),
// or the frames left of the brief immunity after a resurrection
int GetEntityImmunity ( lua_State *L )
{
	lua = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	int iImmunity = 0;
	if ( LuaEntityIDValid ( L, e, 1 ) ) iImmunity = t.entityelement[e].briefimmunity;
	lua_pushinteger ( L, iImmunity );
	return 1;
}

int SetAttachmentVisible ( lua_State *L )
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 2 )
		return 0;

	int e = lua_tointeger(L, 1);
	int v = lua_tointeger(L, 2);
	if ( !LuaEntityIDValid ( L, e, 1 ) ) return 0;

	if ( v == 1 )
	{
		if ( t.entityelement[e].attachmentobj > 0 )
		{
			if ( ObjectExist ( t.entityelement[e].attachmentobj ) == 1 )
				ShowObject ( t.entityelement[e].attachmentobj );
		}
	}
	else
	{
		if ( t.entityelement[e].attachmentobj > 0 )
		{
			if ( ObjectExist ( t.entityelement[e].attachmentobj ) == 1 )
				HideObject ( t.entityelement[e].attachmentobj );
		}
	}
	return 0;
}

int Include(lua_State *L)
{
	lua = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 1 ) return 0;

	char str[MAX_PATH];
	strcpy ( str , "scriptbank\\" );
	strcat ( str , lua_tostring(L, 1) );

	LoadLua ( str );

	return 0;
}

#ifdef WICKEDENGINE
int GetCharacterForwardX(lua_State* L)
{
	GGVECTOR3 forward;
	ODEGetCharacterWalkDirection(&forward);
	lua_pushnumber(L, forward.x);
	return 1;
}
int GetCharacterForwardY(lua_State* L)
{
	GGVECTOR3 forward;
	ODEGetCharacterWalkDirection(&forward);
	lua_pushnumber(L, forward.y);
	return 1;
}
int GetCharacterForwardZ(lua_State* L)
{
	GGVECTOR3 forward;
	ODEGetCharacterWalkDirection(&forward);
	lua_pushnumber(L, forward.z);
	return 1;
}
#endif

// material commands
int SetMaterialData(lua_State *L, int mode)
{
	lua = L;
	int n = LUA_GETTOP(L);
	int iEntityID = lua_tonumber(L, 1);
	if (!LuaEntityIDValid(L, iEntityID, 1)) return 0;
	int iObjID = t.entityelement[iEntityID].obj;
	if (!ConfirmObjectInstance (iObjID)) return 0;
	sObject* pObject = g_ObjectList[iObjID];
	if (mode == 0 || mode == 6)
	{
		if (n < 4) return 0;
		float fValueR = lua_tonumber(L, 2);
		float fValueG = lua_tonumber(L, 3);
		float fValueB = lua_tonumber(L, 4);
		if (mode == 0) WickedCall_SetObjectBaseColor(pObject, fValueR, fValueG, fValueB);
		if (mode == 6) WickedCall_SetObjectEmissiveColor(pObject, fValueR, fValueG, fValueB);
	}
	else
	{
		if (mode <= 21)
		{
			if (n < 2) return 0;
			float fValue = lua_tonumber(L, 2);
			switch (mode)
			{
				case 1: WickedCall_SetObjectAlpha (pObject, fValue); break;
				case 2: WickedCall_SetObjectAlphaRef (pObject, fValue / 100.0f); break;
				case 3: WickedCall_SetObjectNormalness (pObject, fValue / 100.0f); break;
				case 4: WickedCall_SetObjectRoughness (pObject, fValue / 100.0f); break;
				case 5: WickedCall_SetObjectMetalness (pObject, fValue / 100.0f); break;
				case 7: WickedCall_SetObjectEmissiveStrength (pObject, fValue / 100.0f); break;
				case 8: WickedCall_SetObjectReflectance (pObject, fValue / 100.0f); break;
				case 9: WickedCall_SetObjectRenderOrderBias (pObject, fValue / 100.0f); break;
				case 10: WickedCall_SetObjectTransparentDirect (pObject, (bool)fValue); break;
				case 11: WickedCall_SetObjectDoubleSided (pObject, (bool)fValue); break;
				case 12: WickedCall_SetObjectPlanerReflection (pObject, (bool)fValue); break;
				case 13: WickedCall_SetObjectCastShadows(pObject, (bool)fValue); break;
				case 14:
				{
					// cannot hold per instance of zdepth mode, but can force object to a mode if we usurp entityprofile temporarily
					// the state will be reset when the level exits
					int entid = t.entityelement[iEntityID].bankindex;
					int storezdepthmode = t.entityprofile[entid].zdepth;
					t.entityprofile[entid].zdepth = (int)fValue;
					entity_preparedepth(entid, iObjID);
					t.entityprofile[entid].zdepth = storezdepthmode;
					break;
				}
				// 21-30 texture commands
				case 21:
				{
					// Before changing the basecolor texture, ensure we remove all other textures for this object, to trigger the WickedCall_TextureObject() function to try and find the new ones.
					WickedCall_RemoveObjectTextures(pObject);

					// change texture of this object 
					int iImageID = (int)fValue;
					TextureObject(t.entityelement[iEntityID].obj, iImageID);
					WickedCall_TextureObject(pObject, NULL);
					break;
				}
				case 15:
				{
					WickedCall_SetObjectOutline(pObject, fValue);
					break;
				}

			}
		}
		else
		{
			if (n < 3) return 0;
			float fValue1 = lua_tonumber(L, 2);
			float fValue2 = lua_tonumber(L, 3);
			switch (mode)
			{
				case 22:
				{
					// change texture UV scale of this object
					float x, y, z, w;
					WickedCall_GetObjectTextureUV(pObject, &x, &y, &z, &w);
					WickedCall_SetObjectTextureUV(pObject, fValue1, fValue2, z, w);
					break;
				}
				case 23:
				{
					// change texture UV offset of this object
					float x, y, z, w;
					WickedCall_GetObjectTextureUV(pObject, &x, &y, &z, &w);
					WickedCall_SetObjectTextureUV(pObject, x, y, fValue1, fValue2);
					break;
				}
				// Wicked Can auto animate with texAnimDirection and texAnimFrameRate
			}
		}
	}
	return 0;
}
int GetMaterialData(lua_State *L, int mode)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iEntityID = lua_tonumber(L, 1);
	if (!LuaEntityIDValid(L, iEntityID, 1)) return 0;
	int iObjID = t.entityelement[iEntityID].obj;
	if (!ConfirmObjectInstance (iObjID)) return 0;
	sObject* pObject = g_ObjectList[iObjID];
	if (mode == 0 || mode == 6)
	{
		int iResultR = 0.0f;
		int iResultG = 0.0f;
		int iResultB = 0.0f;
		if (mode == 0) WickedCall_GetObjectBaseColor(pObject, &iResultR, &iResultG, &iResultB);
		if (mode == 6) WickedCall_GetObjectEmissiveColor(pObject, &iResultR, &iResultG, &iResultB);
		lua_pushnumber(L, iResultR);
		lua_pushnumber(L, iResultG);
		lua_pushnumber(L, iResultB);
		return 3;
	}
	else
	{
		float fResult = 0.0f;
		switch (mode)
		{
			case 1: fResult = WickedCall_GetObjectAlpha (pObject); break;
			case 2: fResult = WickedCall_GetObjectAlphaRef (pObject) * 100.0f; break;
			case 3: fResult = WickedCall_GetObjectNormalness (pObject) * 100.0f; break;
			case 4: fResult = WickedCall_GetObjectRoughness (pObject) * 100.0f; break;
			case 5: fResult = WickedCall_GetObjectMetalness (pObject) * 100.0f; break;
			case 7: fResult = WickedCall_GetObjectEmissiveStrength (pObject) * 100.0f; break;
			case 8: fResult = WickedCall_GetObjectReflectance (pObject) * 100.0f; break;
			case 9: fResult = WickedCall_GetObjectRenderOrderBias (pObject) * 100.0f; break;
			case 10: fResult = (float)WickedCall_GetObjectTransparentDirect (pObject); break;
			case 11: fResult = (float)WickedCall_GetObjectDoubleSided (pObject); break;
			case 12: fResult = (float)WickedCall_GetObjectPlanerReflection (pObject); break;
			case 13: fResult = (float)WickedCall_GetObjectCastShadows(pObject); break;
			case 14: fResult = (float)t.entityprofile[t.entityelement[iEntityID].bankindex].zdepth; break;
			case 15: fResult = (float)WickedCall_GetObjectOutline(pObject); break;

			// reserve 21-30
		}
		lua_pushnumber(L, fResult);
		return 1;
	}
}
// ResetEntityMaterial(e): puts back the entity's materials as the level load gave them, mesh by mesh (its .fpe's, or its
// element's own): textures, base and emissive colour, emissive strength, roughness, metalness, reflectance, normal
// strength, alpha and transparency. Like the material setters it changes every entity of the model, as they share their
// materials; the per-instance values (SetEntityInstanceEmissive, the tint, limb alpha) are left alone
int ResetEntityMaterial (lua_State *L)
{
	lua = L;
	if (LUA_GETTOP(L) < 1) return 0;
	int iEntityID = lua_tointeger(L, 1);
	if (!LuaEntityIDValid(L, iEntityID, 1)) return 0;
	entity_resetmaterial(iEntityID);
	return 0;
}
// the entity's mesh (material slot) number slot, 0 to GetEntityMeshCount(e) - 1, or NULL
static sMesh* LuaEntityMesh (lua_State *L, int iEntityID, int iSlot)
{
	if (!LuaEntityIDValid(L, iEntityID, 1)) return NULL;
	int iObjID = t.entityelement[iEntityID].obj;
	if (!ConfirmObjectInstance (iObjID)) return NULL;
	sObject* pObject = g_ObjectList[iObjID];
	if (!pObject || !pObject->ppMeshList || iSlot < 0 || iSlot >= pObject->iMeshCount) return NULL;
	return pObject->ppMeshList[iSlot];
}
// GetEntityMeshCount(e): how many meshes (material slots) the entity's object has, numbered from 0
int GetEntityMeshCount (lua_State *L)
{
	lua = L;
	int iCount = 0;
	if (LUA_GETTOP(L) >= 1)
	{
		int iEntityID = lua_tointeger(L, 1);
		if (LuaEntityIDValid(L, iEntityID, 1))
		{
			int iObjID = t.entityelement[iEntityID].obj;
			if (ConfirmObjectInstance (iObjID) && g_ObjectList[iObjID]) iCount = g_ObjectList[iObjID]->iMeshCount;
		}
	}
	lua_pushinteger(L, iCount);
	return 1;
}
// SetEntityMeshBaseColor(e, slot, r, g, b) / SetEntityMeshEmissiveColor(e, slot, r, g, b): one material slot's colour,
// 0 to 255, where SetEntityBaseColor / SetEntityEmissiveColor set every slot's. Model-wide, as those are
static int SetEntityMeshColor (lua_State *L, bool bEmissive)
{
	lua = L;
	if (LUA_GETTOP(L) < 5) return 0;
	sMesh* pMesh = LuaEntityMesh(L, lua_tointeger(L, 1), lua_tointeger(L, 2));
	if (!pMesh) return 0;
	int r = lua_tointeger(L, 3), g = lua_tointeger(L, 4), b = lua_tointeger(L, 5);
	if (bEmissive) WickedCall_SetMeshEmissiveColor(pMesh, r, g, b); else WickedCall_SetMeshBaseColor(pMesh, r, g, b);
	return 0;
}
// GetEntityMeshBaseColor(e, slot) / GetEntityMeshEmissiveColor(e, slot): one material slot's colour, r, g, b (0 to 255),
// where GetEntityBaseColor / GetEntityEmissiveColor read only slot 0's
static int GetEntityMeshColor (lua_State *L, bool bEmissive)
{
	lua = L;
	int r = 0, g = 0, b = 0;
	if (LUA_GETTOP(L) >= 2)
	{
		sMesh* pMesh = LuaEntityMesh(L, lua_tointeger(L, 1), lua_tointeger(L, 2));
		if (pMesh)
		{
			if (bEmissive) WickedCall_GetMeshEmissiveColor(pMesh, &r, &g, &b); else WickedCall_GetMeshBaseColor(pMesh, &r, &g, &b);
		}
	}
	lua_pushinteger(L, r);
	lua_pushinteger(L, g);
	lua_pushinteger(L, b);
	return 3;
}
int SetEntityMeshBaseColor (lua_State *L) { return SetEntityMeshColor (L, false); }
int GetEntityMeshBaseColor (lua_State *L) { return GetEntityMeshColor (L, false); }
int SetEntityMeshEmissiveColor (lua_State *L) { return SetEntityMeshColor (L, true); }
int GetEntityMeshEmissiveColor (lua_State *L) { return GetEntityMeshColor (L, true); }
int SetEntityBaseColor (lua_State *L) { return SetMaterialData (L, 0); }
int GetEntityBaseColor (lua_State *L) { return GetMaterialData (L, 0); }
int SetEntityBaseAlpha (lua_State *L) { return SetMaterialData (L, 1); }
int GetEntityBaseAlpha (lua_State *L) { return GetMaterialData (L, 1); }
int SetEntityAlphaClipping (lua_State *L) { return SetMaterialData (L, 2); }
int GetEntityAlphaClipping (lua_State *L) { return GetMaterialData (L, 2); }
int SetEntityNormalStrength (lua_State *L) { return SetMaterialData (L, 3); }
int GetEntityNormalStrength (lua_State *L) { return GetMaterialData (L, 3); }
int SetEntityRoughnessStrength (lua_State *L) { return SetMaterialData (L, 4); }
int GetEntityRoughnessStrength (lua_State *L) { return GetMaterialData (L, 4); }
int SetEntityMetalnessStrength (lua_State *L) { return SetMaterialData (L, 5); }
int GetEntityMetalnessStrength (lua_State *L) { return GetMaterialData (L, 5); }
int SetEntityEmissiveColor (lua_State *L) { return SetMaterialData (L, 6); }
int GetEntityEmissiveColor (lua_State *L) { return GetMaterialData (L, 6); }
int SetEntityEmissiveStrength (lua_State *L) { return SetMaterialData (L, 7); }
int GetEntityEmissiveStrength (lua_State *L) { return GetMaterialData (L, 7); }
int SetEntityReflectance (lua_State *L) { return SetMaterialData (L, 8); }
int GetEntityReflectance (lua_State *L) { return GetMaterialData (L, 8); }
int SetEntityRenderBias (lua_State *L) { return SetMaterialData (L, 9); }
int GetEntityRenderBias (lua_State *L) { return GetMaterialData (L, 9); }
int SetEntityTransparency (lua_State *L) { return SetMaterialData (L, 10); }
int GetEntityTransparency (lua_State *L) { return GetMaterialData (L, 10); }
int SetEntityDoubleSided (lua_State *L) { return SetMaterialData (L, 11); }
int GetEntityDoubleSided (lua_State *L) { return GetMaterialData (L, 11); }
int SetEntityPlanarReflection (lua_State *L) { return SetMaterialData (L, 12); }
int GetEntityPlanarReflection (lua_State *L) { return GetMaterialData (L, 12); }
int SetEntityCastShadows (lua_State *L) { return SetMaterialData (L, 13); }
int GetEntityCastShadows (lua_State *L) { return GetMaterialData (L, 13); }
int SetEntityZDepthMode (lua_State *L) { return SetMaterialData (L, 14); }
int GetEntityZDepthMode (lua_State *L) { return GetMaterialData (L, 14); }

int SetEntityOutline(lua_State* L) { return SetMaterialData(L, 15); }
int GetEntityOutline(lua_State* L) { return GetMaterialData(L, 15); }

int SetEntityTexture (lua_State *L) { return SetMaterialData (L, 21); }
int SetEntityTextureScale (lua_State *L) { return SetMaterialData (L, 22); }
int SetEntityTextureOffset (lua_State *L) { return SetMaterialData (L, 23); }

int GetEntityInZoneWithFilter(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;

	int storee = t.e;
	int storev = t.v;
	if (n == 1)
	{
		t.v = lua_tonumber(L, 1);
		t.e = t.v; // old legacy code confused LuaMessageInt() and LuaMessageIndex(), assigning to t.e and t.v arbitarily!
		entity_lua_getentityinzone(0);
	}
	if (n == 2)
	{
		t.e = lua_tonumber(L, 1);
		t.v = lua_tonumber(L, 2);
		entity_lua_getentityinzone(t.v);
	}

	t.e = storee;
	t.v = storev;
	return 0;

}

int IsPointWithinZone(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iIsInZone = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if(LuaEntityIDValid(L, iEntityIndex, 1))
	{
		float fX = lua_tonumber(L, 2);
		float fY = lua_tonumber(L, 3);
		float fZ = lua_tonumber(L, 4);
		int waypointindex = t.entityelement[iEntityIndex].eleprof.trigger.waypointzoneindex;
		if (waypointindex > 0)
		{
			if (t.waypoint[waypointindex].active == 1)
			{
				if (t.waypoint[waypointindex].style == 2)
				{
					t.tpointx_f = fX;
					t.tpointz_f = fZ;
					t.tokay = 0; waypoint_ispointinzone ();
					if (t.tokay != 0)
					{
						// the specified point is within specified entity zone
						iIsInZone = 1;
					}
				}
			}
		}
	}
	lua_pushnumber(L, iIsInZone);
	return 1;
}

int SetWeaponArmsVisible(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;

	extern bool bHideWeaponsMuzzle;
	extern bool bHideWeaponsSmoke;
	extern bool bHideWeapons;
	extern int iTracerPosition;
	iTracerPosition = 0;
	bHideWeapons = lua_tonumber(L, 1);
	if (n == 2)
	{
		bHideWeaponsMuzzle = lua_tonumber(L, 2);
	}
	if (n >= 3)
	{
		bHideWeaponsMuzzle = lua_tonumber(L, 2);
		bHideWeaponsSmoke = lua_tonumber(L, 3);
	}
	if (n >= 4)
	{
		iTracerPosition = lua_tonumber(L, 4);
	}

	return 1;
}

int GetWeaponArmsVisible(lua_State* L)
{
	lua = L;
	extern bool bHideWeapons;
	lua_pushboolean(L, bHideWeapons);
	return 1;
}

int IsPlayerInGame(lua_State* L) 
{ 
	bool bInGame = t.game.gameloop > 0 && t.game.titleloop == 0; 
	lua_pushboolean(L, bInGame);
	return 1;
}

int SetLevelFadeoutEnabled(lua_State* L)
{
	lua = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int newFadeoutState = lua_tonumber(L, 1);
	t.postprocessings.fadeinenabled = newFadeoutState;
	return 1;
}

extern void WickedCall_SetSunColors(float fRed, float fGreen, float fBlue, float fEnergy, float fFov, float fShadowBias);
int SetSunLightingColor(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float r = lua_tonumber(L, 1);
	float g = lua_tonumber(L, 2);
	float b = lua_tonumber(L, 3);
	t.visuals.SunRed_f = r;
	t.visuals.SunGreen_f = g;
	t.visuals.SunBlue_f = b;
	WickedCall_SetSunColors(r, g, b, t.visuals.SunIntensity_f, 1.0f, 1.0f);
	return 1;
}

int SetSunIntensity(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float intensity = lua_tonumber(L, 1);
	t.visuals.SunIntensity_f = intensity;
	WickedCall_SetSunColors(t.visuals.SunRed_f, t.visuals.SunGreen_f, t.visuals.SunBlue_f, t.visuals.SunIntensity_f, 1.0f, 1.0f);
	return 1;
}

int GetSunColorRed(lua_State* L)
{
	lua_pushnumber(L, t.visuals.SunRed_f);
	return 1;
}
int GetSunColorGreen(lua_State* L)
{
	lua_pushnumber(L, t.visuals.SunGreen_f);
	return 1;
}
int GetSunColorBlue(lua_State* L)
{
	lua_pushnumber(L, t.visuals.SunBlue_f);
	return 1;
}

int GetSunIntensity(lua_State* L)
{
	lua_pushnumber(L, t.visuals.SunIntensity_f);
	return 1;
}

int SetExposure(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float exposure = lua_tonumber(L, 1);
	t.visuals.fExposure = exposure;
	WickedCall_SetExposure(exposure);
	return 1;
}

int GetExposure(lua_State* L)
{
	lua_pushnumber(L, t.visuals.fExposure);
	return 1;
}

// post effects for the running game (M-GridEditB.cpp LuaPostEffects_*); an omitted or negative argument keeps the
// current value, and the level's own settings come back at the next level or when a test game ends
extern void LuaPostEffects_SetBloom(int iEnabled, float fStrength, float fThreshold);
extern void LuaPostEffects_GetBloom(int* piEnabled, float* pfStrength, float* pfThreshold);
extern void LuaPostEffects_SetDepthOfField(int iEnabled, float fStrength, float fFocalLength, float fApertureSize);
extern void LuaPostEffects_GetDepthOfField(int* piEnabled, float* pfStrength, float* pfFocalLength, float* pfApertureSize);
extern void LuaPostEffects_SetLightShafts(int iEnabled);
extern void LuaPostEffects_SetLensFlare(int iEnabled);
extern bool GetLightShaftState(void);
extern bool GetLensFlareState(void);

float LuaOptionalNumber(lua_State* L, int n, int index)
{
	if (n < index) return -1.0f;
	return lua_tonumber(L, index);
}

int SetBloom(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaPostEffects_SetBloom((int)lua_tonumber(L, 1), LuaOptionalNumber(L, n, 2), LuaOptionalNumber(L, n, 3));
	return 0;
}

int GetBloom(lua_State* L)
{
	int iEnabled = 0;
	float fStrength = 0, fThreshold = 0;
	LuaPostEffects_GetBloom(&iEnabled, &fStrength, &fThreshold);
	lua_pushnumber(L, iEnabled);
	lua_pushnumber(L, fStrength);
	lua_pushnumber(L, fThreshold);
	return 3;
}

int SetDepthOfField(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaPostEffects_SetDepthOfField((int)lua_tonumber(L, 1), LuaOptionalNumber(L, n, 2), LuaOptionalNumber(L, n, 3), LuaOptionalNumber(L, n, 4));
	return 0;
}

int GetDepthOfField(lua_State* L)
{
	int iEnabled = 0;
	float fStrength = 0, fFocalLength = 0, fApertureSize = 0;
	LuaPostEffects_GetDepthOfField(&iEnabled, &fStrength, &fFocalLength, &fApertureSize);
	lua_pushnumber(L, iEnabled);
	lua_pushnumber(L, fStrength);
	lua_pushnumber(L, fFocalLength);
	lua_pushnumber(L, fApertureSize);
	return 4;
}

int SetLightShafts(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaPostEffects_SetLightShafts((int)lua_tonumber(L, 1));
	return 0;
}

int GetLightShafts(lua_State* L)
{
	lua_pushnumber(L, GetLightShaftState() ? 1 : 0);
	return 1;
}

int SetLensFlare(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaPostEffects_SetLensFlare((int)lua_tonumber(L, 1));
	return 0;
}

int GetLensFlare(lua_State* L)
{
	lua_pushnumber(L, GetLensFlareState() ? 1 : 0);
	return 1;
}

// performance levers for the running game (M-GridEditB.cpp LuaRenderSettings_*): kept through visuals pushes and
// graphics quality changes, and back to the level's own at the next level or when a test game ends. An omitted or
// negative argument keeps the current value
extern void LuaRenderSettings_SetOcclusionCulling(int iOn, int iObjects, int iAnimations, int iTerrain, int iShadows);
extern void LuaRenderSettings_SetLODMultiplier(float fMultiplier);
extern void LuaRenderSettings_SetShadowsLowestLOD(int iOn);
extern void LuaRenderSettings_SetDelayedShadows(int iOn, int iLaptop);
extern void LuaRenderSettings_ResetDelayedShadows(void);
extern void LuaRenderSettings_GetDelayedShadows(int* pOn, int* pLaptop, int* pScript);
extern void LuaRenderSettings_SetShadowRange(float fRange);
extern void LuaRenderSettings_SetSSR(int iOn);
extern void LuaRenderSettings_SetAO(int iOn, float fPower);
extern void LuaRenderSettings_SetSpotShadowCulling(int iOn);
extern void LuaRenderSettings_SetFXAA(int iOn);
extern void LuaRenderSettings_SetReflections(int iOn);
extern void LuaRenderSettings_SetLowestLOD(int iProbes, int iReflections);
extern void LuaRenderSettings_SetAnimations30Fps(int iOn);
extern void LuaRenderSettings_SetMaxApparentSize(float fSize);
extern void LuaRenderSettings_SetShadowResolution(int iSun, int iSpot, int iPoint);
extern void LuaRenderSettings_SetShadowLights(int iSpotMax, int iPointMax);
extern void LuaRenderSettings_SetShadowCascades(int iCascades, const float* pSplits);
extern void LuaRenderSettings_SetTerrainDetail(int iLimit, float fScale, int iReadBack);
extern void LuaRenderSettings_SetGrassSimpleLighting(int iOn);
extern void LuaRenderSettings_ResetGraphics(void);
extern void LuaRenderSettings_SetShadowJobWait(int iOn);
extern void LuaRenderSettings_SetVsync(int iOn);
extern void LuaRenderSettings_SetFrameRateCap(int iFramesPerSecond);
extern void LuaRenderSettings_GetDisplay(int* pVsync, int* pFrameRateCap);
extern void LuaRenderSettings_SetRenderScale(float fScale);
extern float LuaRenderSettings_GetRenderScale(void);
extern void LuaRenderSettings_GetLevers(int* pSSR, int* pAO, float* pAOPower, float* pLODMultiplier, float* pShadowRange);
extern void LuaRenderSettings_GetValues(int* pFXAA, int* pReflections, int* pProbesLowestLOD, int* pReflectionsLowestLOD, int* pAnimations30Fps, float* pMaxApparentSize);
extern void LuaRenderSettings_GetShadows(int* pSun, int* pSpot, int* pPoint, int* pSpotMax, int* pPointMax, int* pCascades, float* pSplits);
extern void LuaRenderSettings_GetTerrainDetail(int* pLimit, float* pScale, int* pReadBack, int* pGrassSimpleLighting);

// SetOcclusionCulling(on [, objects [, animations [, terrain [, shadows [, spotShadows]]]]]): the GPU occlusion culling,
// which skips what stands wholly behind something (a frame late, and nothing within 1500 units), and what it is used for:
// objects, the animation of an occluded character (it pauses), terrain chunks, and point and spot light shadows (shadows
// both, or point only when spotShadows is given). 1 or 0 each
int SetOcclusionCulling(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetOcclusionCulling((int)lua_tonumber(L, 1), (int)LuaOptionalNumber(L, n, 2), (int)LuaOptionalNumber(L, n, 3), (int)LuaOptionalNumber(L, n, 4), (int)LuaOptionalNumber(L, n, 5));
	if (n >= 6) LuaRenderSettings_SetSpotShadowCulling((int)lua_tonumber(L, 6));
	return 0;
}

// SetLODMultiplier(v): models with LOD levels switch to LOD 1, 2 and 3 past 400, 600 and 800 units times v (0 to 15; the
// graphics quality sets 1 to 3)
int SetLODMultiplier(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetLODMultiplier(lua_tonumber(L, 1));
	return 0;
}

// SetShadowsLowestLOD(on): models cast their shadows from their lowest LOD, which is cheaper
int SetShadowsLowestLOD(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetShadowsLowestLOD((int)lua_tonumber(L, 1));
	return 0;
}

// SetDelayedShadows(on [, laptop]): the sun's shadow cascades 1 to 4 are redrawn every 2, 3, 4 and 9 frames instead of
// every frame, and point light shadows less often too; laptop also redraws cascade 0 every other frame and the rest every
// 3, 4, 5 and 9. On unless the level turned it off
int SetDelayedShadows(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetDelayedShadows((int)lua_tonumber(L, 1), (int)LuaOptionalNumber(L, n, 2));
	return 0;
}

// ResetDelayedShadows(): forget what SetDelayedShadows set and go back to the level's or the graphics quality's choice
int ResetDelayedShadows(lua_State* L)
{
	LuaRenderSettings_ResetDelayedShadows();
	return 0;
}

// GetDelayedShadows(): on, laptop as in force now, and script 1 while a SetDelayedShadows value holds
int GetDelayedShadows(lua_State* L)
{
	int iOn, iLaptop, iScript;
	LuaRenderSettings_GetDelayedShadows(&iOn, &iLaptop, &iScript);
	lua_pushinteger(L, iOn);
	lua_pushinteger(L, iLaptop);
	lua_pushinteger(L, iScript);
	return 3;
}

// the rest of what the graphics quality sets, for a graphics menu with its own options (kept through visuals pushes and
// quality changes, the level's own again at the next level and when a test game ends; a negative value keeps the current)
// SetFXAA(on), SetReflections(on): the FXAA pass and the planar reflections
int SetFXAA(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetFXAA((int)lua_tonumber(L, 1));
	return 0;
}
int SetReflections(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetReflections((int)lua_tonumber(L, 1));
	return 0;
}
// SetProbesLowestLOD(on), SetReflectionsLowestLOD(on): environment probes and reflections drawn from each model's lowest LOD
int SetProbesLowestLOD(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetLowestLOD((int)lua_tonumber(L, 1), -1);
	return 0;
}
int SetReflectionsLowestLOD(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetLowestLOD(-1, (int)lua_tonumber(L, 1));
	return 0;
}
// SetAnimations30Fps(on): far characters animate at 30 frames a second
int SetAnimations30Fps(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetAnimations30Fps((int)lua_tonumber(L, 1));
	return 0;
}
// SetMaxApparentSize(v): objects smaller on screen than this (0.000001 to 0.2, the editor's value / 10000) aren't drawn
int SetMaxApparentSize(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetMaxApparentSize((float)lua_tonumber(L, 1));
	return 0;
}
// SetShadowResolution(sun [, spot [, point]]): shadow map sizes, 0 (none) to 2048; a change recreates the maps (a hitch)
int SetShadowResolution(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetShadowResolution((int)lua_tonumber(L, 1), (int)LuaOptionalNumber(L, n, 2), (int)LuaOptionalNumber(L, n, 3));
	return 0;
}
// SetShadowLights(spotMax [, pointMax]): how many spot and point lights cast shadows at once, 0 to 16
int SetShadowLights(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetShadowLights((int)lua_tonumber(L, 1), (int)LuaOptionalNumber(L, n, 2));
	return 0;
}
// SetShadowCascades(n [, split1, split2, split3, split4]): the sun's shadow cascades in use, 1 to 5 (past the last, no sun
// shadow), and the view depths the first four end at (380, 950, 7500, 30000 unless set; used only if all four are given,
// above 0 and rising). Moving a split changes that cascade's texel size, so its shadows sharpen or blur; each cascade's depth
// bias was tuned to the stock splits, so acne or light leaks may show
int SetShadowCascades(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float fSplits[4];
	bool bSplits = n >= 5;
	for (int i = 0; i < 4 && bSplits; i++) fSplits[i] = (float)lua_tonumber(L, 2 + i);
	LuaRenderSettings_SetShadowCascades((int)lua_tonumber(L, 1), bSplits ? fSplits : NULL);
	return 0;
}
// SetTerrainDetail(limit [, scale [, readBack]]): the terrain texture's detail as the graphics quality sets it: the detail
// limit 0 (finest) to 4, the detail scale 0.25 to 2, the read back reduction 4 to 6 (higher reads back less, coarser pages)
int SetTerrainDetail(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetTerrainDetail((int)lua_tonumber(L, 1), (float)LuaOptionalNumber(L, n, 2), (int)LuaOptionalNumber(L, n, 3));
	return 0;
}
// SetGrassSimpleLighting(on): the grass lit by the simpler, cheaper PBR (Low and Medium quality)
int SetGrassSimpleLighting(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetGrassSimpleLighting((int)lua_tonumber(L, 1));
	return 0;
}
// SetShadowJobWait(on): the shadow job waits for the frame set up and prepass jobs even with delayed shadows off, as it
// does with them on (off by default; for the stalls in Wicked's Render with delayed shadows off). Back to off at the next
// level and test end
int SetShadowJobWait(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetShadowJobWait((int)lua_tonumber(L, 1));
	return 0;
}
// SetVsync(on): vsync on (1) or off (0), outranking the level's and SETUP.INI's; the swap chain is made again (a blip)
int SetVsync(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetVsync((int)lua_tonumber(L, 1));
	return 0;
}
// SetFrameRateCap(fps): the most frames a second, 10 to 1000; 0 for no cap
int SetFrameRateCap(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetFrameRateCap((int)lua_tonumber(L, 1));
	return 0;
}
// SetRenderScale(f): the 3D drawn at f of the screen's resolution, 0.5 to 1, and upscaled with FSR 1, the HUD and sprites
// at the screen's; FXAA stays on below 1. It stands in for the level's FSR setting; 0 goes back to it. The render targets
// are made again (a hitch), so set it when the player lets go of a slider, not every step
int SetRenderScale(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetRenderScale(lua_tonumber(L, 1));
	return 0;
}
// GetRenderScale(): the fraction of the screen's resolution the 3D is drawn at (the level's FSR setting's when no script set it)
int GetRenderScale(lua_State* L)
{
	lua_pushnumber(L, LuaRenderSettings_GetRenderScale());
	return 1;
}
// ResetGraphicsSettings(): forget every value the graphics setters above set and go back to the level's and the graphics
// quality's, so a quality change moves them again (a menu's Default)
int ResetGraphicsSettings(lua_State* L)
{
	LuaRenderSettings_ResetGraphics();
	return 0;
}
// GetGraphicsSettings(): the values in force as a table: fxaa, reflections, probesLowestLOD, reflectionsLowestLOD,
// animations30Fps, maxApparentSize, sunShadow, spotShadow, pointShadow (resolutions), spotShadowLights, pointShadowLights,
// shadowCascades, shadowSplits (a table of four), terrainDetailLimit, terrainDetailScale, terrainReadBack, grassSimpleLighting
int GetGraphicsSettings(lua_State* L)
{
	int iFXAA, iReflections, iProbes, iReflectionsLOD, i30Fps;
	float fApparentSize;
	LuaRenderSettings_GetValues(&iFXAA, &iReflections, &iProbes, &iReflectionsLOD, &i30Fps, &fApparentSize);
	int iSun, iSpot, iPoint, iSpotMax, iPointMax, iCascades;
	float fSplits[4];
	LuaRenderSettings_GetShadows(&iSun, &iSpot, &iPoint, &iSpotMax, &iPointMax, &iCascades, fSplits);
	int iLimit, iReadBack, iGrassSimple;
	float fScale;
	LuaRenderSettings_GetTerrainDetail(&iLimit, &fScale, &iReadBack, &iGrassSimple);
	int iVsync, iFrameRateCap;
	LuaRenderSettings_GetDisplay(&iVsync, &iFrameRateCap);
	lua_newtable(L);
	lua_pushinteger(L, iFXAA); lua_setfield(L, -2, "fxaa");
	lua_pushinteger(L, iReflections); lua_setfield(L, -2, "reflections");
	lua_pushinteger(L, iProbes); lua_setfield(L, -2, "probesLowestLOD");
	lua_pushinteger(L, iReflectionsLOD); lua_setfield(L, -2, "reflectionsLowestLOD");
	lua_pushinteger(L, i30Fps); lua_setfield(L, -2, "animations30Fps");
	lua_pushnumber(L, fApparentSize); lua_setfield(L, -2, "maxApparentSize");
	lua_pushinteger(L, iSun); lua_setfield(L, -2, "sunShadow");
	lua_pushinteger(L, iSpot); lua_setfield(L, -2, "spotShadow");
	lua_pushinteger(L, iPoint); lua_setfield(L, -2, "pointShadow");
	lua_pushinteger(L, iSpotMax); lua_setfield(L, -2, "spotShadowLights");
	lua_pushinteger(L, iPointMax); lua_setfield(L, -2, "pointShadowLights");
	lua_pushinteger(L, iCascades); lua_setfield(L, -2, "shadowCascades");
	lua_newtable(L);
	for (int i = 0; i < 4; i++) { lua_pushnumber(L, fSplits[i]); lua_rawseti(L, -2, i + 1); }
	lua_setfield(L, -2, "shadowSplits");
	lua_pushinteger(L, iLimit); lua_setfield(L, -2, "terrainDetailLimit");
	lua_pushnumber(L, fScale); lua_setfield(L, -2, "terrainDetailScale");
	lua_pushinteger(L, iReadBack); lua_setfield(L, -2, "terrainReadBack");
	lua_pushinteger(L, iGrassSimple); lua_setfield(L, -2, "grassSimpleLighting");
	lua_pushinteger(L, iVsync); lua_setfield(L, -2, "vsync");
	lua_pushinteger(L, iFrameRateCap); lua_setfield(L, -2, "frameRateCap");
	lua_pushnumber(L, LuaRenderSettings_GetRenderScale()); lua_setfield(L, -2, "renderScale");
	int iSSR, iAO;
	float fAOPower, fLODMultiplier, fShadowRange;
	LuaRenderSettings_GetLevers(&iSSR, &iAO, &fAOPower, &fLODMultiplier, &fShadowRange);
	lua_pushinteger(L, iSSR); lua_setfield(L, -2, "ssr");
	lua_pushinteger(L, iAO); lua_setfield(L, -2, "ao");
	lua_pushnumber(L, fAOPower); lua_setfield(L, -2, "aoPower");
	lua_pushnumber(L, fLODMultiplier); lua_setfield(L, -2, "lodMultiplier");
	lua_pushnumber(L, fShadowRange); lua_setfield(L, -2, "shadowRange");
	return 1;
}

// SetShadowRange(d): how far the sun's last shadow cascade reaches, 31000 to 500000 units (500000 unless set); nothing
// beyond it casts a sun shadow, and nearer makes the far shadows sharper. 0 goes back to the level's
int SetShadowRange(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetShadowRange(lua_tonumber(L, 1));
	return 0;
}

// SetSSR(on): screen space reflections on (1) or off (0)
int SetSSR(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetSSR((int)lua_tonumber(L, 1));
	return 0;
}

// SetAO(on [, power]): ambient occlusion (corners and gaps take less ambient light) on (1) or off (0), and its power, 0
// to 5 (1 is the editor's default)
int SetAO(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetAO((int)lua_tonumber(L, 1), LuaOptionalNumber(L, n, 2));
	return 0;
}

// the weather wind (rain, snow and Wicked wind; tree wind is SetTreeWind). Like the ambience and fog setters it changes
// t.visuals, which a test game puts back when it ends
extern void Wicked_Update_Wind(void* visual);

// SetWind(speed [, dirX, dirY, dirZ [, randomness]]): omitted arguments keep their values; ranges as the editor's sliders.
// Kept in t.gamevisuals like the cloud values, so a test game's wind never reaches the editor
int SetWind(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.wind_speed = max(0.0f, min(5.0f, (float)lua_tonumber(L, 1)));
	if (n >= 4)
	{
		t.gamevisuals.wind_direction_x = max(-20.0f, min(20.0f, (float)lua_tonumber(L, 2)));
		t.gamevisuals.wind_direction_y = max(-20.0f, min(20.0f, (float)lua_tonumber(L, 3)));
		t.gamevisuals.wind_direction_z = max(-20.0f, min(20.0f, (float)lua_tonumber(L, 4)));
	}
	if (n >= 5) t.gamevisuals.wind_randomness = max(0.0f, min(2.0f, (float)lua_tonumber(L, 5)));
	Wicked_Update_Wind((void*)&t.gamevisuals);
	g_bLuaSetWind = true;
	return 0;
}

int GetWind(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.wind_speed);
	lua_pushnumber(L, t.gamevisuals.wind_direction_x);
	lua_pushnumber(L, t.gamevisuals.wind_direction_y);
	lua_pushnumber(L, t.gamevisuals.wind_direction_z);
	lua_pushnumber(L, t.gamevisuals.wind_randomness);
	return 5;
}


bool bActivatePromptXYOffset = false;
bool bActivatePromptOffset3D = false;
int iPromptXOffset = 0;
int iPromptYOffset = 0;
int iPromptZOffset = 0;
int PromptLocalOffset(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int storee = t.e;
	cstr stores = t.s_s;
	if (n == 3)
	{
		iPromptXOffset = lua_tonumber(L, 1);
		iPromptYOffset = lua_tonumber(L, 2);
		iPromptZOffset = lua_tonumber(L, 3);
		bActivatePromptXYOffset = true;
		bActivatePromptOffset3D = true;
	}
	else if (n == 2)
	{
		iPromptXOffset = lua_tonumber(L, 1);
		iPromptYOffset = lua_tonumber(L, 2);
		bActivatePromptXYOffset = true;
		bActivatePromptOffset3D = false;
	}
	t.e = storee;
	t.s_s = stores;
	return 0;
}
	
int PromptGuruMeditation(lua_State * L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.luaglobal.gurumeditationprompttime = Timer();
	t.luaglobal.gurumeditationprompt_s = lua_tostring(L, 1);
}


//gggrass_global_params.grass_scale
int SetGrassScale(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	gggrass_global_params.grass_scale = lua_tonumber(L, 1);
	return 0;
}
int GetGrassScale(lua_State* L)
{
	lua_pushnumber(L, gggrass_global_params.grass_scale);
	return 1;
}
// SetGrassDistance(d) - grass draw distance in world units, clamped to the editor slider range (750 to 7000).
// Changing it rebuilds all grass chunks in one frame, so set it on events rather than every frame.
int SetGrassDistance(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float fDistance = lua_tonumber(L, 1);
	if (fDistance < 750.0f) fDistance = 750.0f;
	if (fDistance > 7000.0f) fDistance = 7000.0f;
	extern float g_fGrassDistanceOverride;
	extern float g_fGrassDistanceBefore;
	if (g_fGrassDistanceOverride <= 0) g_fGrassDistanceBefore = gggrass_global_params.lod_dist;
	g_fGrassDistanceOverride = fDistance;
	gggrass_global_params.lod_dist = fDistance;
	return 0;
}
int GetGrassDistance(lua_State* L)
{
	lua_pushnumber(L, gggrass_global_params.lod_dist);
	return 1;
}
// SetGrassShade(root [, tip]) - a grass blade's light at its root and at its tip, 0 to 2, lerped up the blade; tip left out
// is root. 0.45 both is how the grass has always been drawn (the engine's height term was always 0); 0.45, 1.05 is the
// ramp it was written with. Back to 0.45 at level start and test game end
int SetGrassShade(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float fRoot = (float)lua_tonumber(L, 1);
	float fTip = (n >= 2) ? (float)lua_tonumber(L, 2) : fRoot;
	if (fRoot < 0.0f) fRoot = 0.0f;
	if (fRoot > 2.0f) fRoot = 2.0f;
	if (fTip < 0.0f) fTip = 0.0f;
	if (fTip > 2.0f) fTip = 2.0f;
	GGGrass_SetShade(fRoot, fTip);
	return 0;
}
int GetGrassShade(lua_State* L)
{
	float fRoot, fTip;
	GGGrass_GetShade(&fRoot, &fTip);
	lua_pushnumber(L, fRoot);
	lua_pushnumber(L, fTip);
	return 2;
}
namespace GGTerrain
{
	const char* GGTerrain_GetTextureSource( int slot, int kind, int* pChanged );
}
// GetTextureSource(kind, index [, part]) - the file a vegetation or terrain texture was last uploaded from this session, as
// GG_GetRealPath resolved it (a Documents copy ahead of the install's), and whether that still holds. kind "grass" (index
// the grass slice, 0 to 45), "tree" (index the tree type; part "billboard", the default, "billboardnormal", "trunk" or
// "leaves") or "terrain" (index the material slot, 0 to 31; part "color", the default, "normal" or "surface"). Returns path
// ("" not loaded) and changed: 0 the same file would load now, unchanged; 1 it was written since; 2 another file would load
// now (a Documents copy added or removed); 3 it is gone. Nothing for a bad kind, index or part
// GetEngineProbe(name): the engine time of calls made many times a frame, where GetRenderTime keeps only the first:
// milliseconds and calls a frame, averaged over the last 20 frames, main thread only. name "pick" (a full-accuracy pick:
// IntersectRay, entity rays, bullets), "pick layers" (its render layer changes), "pick wicked" (Wicked's pick),
// "pick lookup" (finding the object hit), "decal create" (decalelement_create: impacts, splashes, rain rings),
// "decal fade" (a decal's alpha change). Nothing for an unknown name
int GetEngineProbe(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	const char* pName = lua_tostring(L, 1);
	float fMilliseconds, fCalls;
	if (!pName || !WickedCall_ProbeGet(pName, &fMilliseconds, &fCalls)) return 0;
	lua_pushnumber(L, fMilliseconds);
	lua_pushnumber(L, fCalls);
	return 2;
}
int GetTextureSource(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	const char* pKind = lua_tostring(L, 1);
	int iIndex = (int)lua_tointeger(L, 2);
	const char* pPart = (n >= 3) ? lua_tostring(L, 3) : NULL;
	if (!pKind) return 0;
	const char* pPath = NULL;
	int iChanged = 0;
	if (stricmp(pKind, "grass") == 0)
	{
		pPath = GGGrass_GetTextureSource(iIndex, &iChanged);
	}
	else if (stricmp(pKind, "tree") == 0)
	{
		int iPart = 0;
		if (pPart)
		{
			if (stricmp(pPart, "billboard") == 0) iPart = 0;
			else if (stricmp(pPart, "billboardnormal") == 0) iPart = 1;
			else if (stricmp(pPart, "trunk") == 0) iPart = 2;
			else if (stricmp(pPart, "leaves") == 0) iPart = 3;
			else return 0;
		}
		pPath = GGTrees::GGTrees_GetTextureSource(iIndex, iPart, &iChanged);
	}
	else if (stricmp(pKind, "terrain") == 0)
	{
		int iPart = 0;
		if (pPart)
		{
			if (stricmp(pPart, "color") == 0) iPart = 0;
			else if (stricmp(pPart, "normal") == 0) iPart = 1;
			else if (stricmp(pPart, "surface") == 0) iPart = 2;
			else return 0;
		}
		pPath = GGTerrain::GGTerrain_GetTextureSource(iIndex, iPart, &iChanged);
	}
	if (!pPath) return 0;
	lua_pushstring(L, pPath);
	lua_pushinteger(L, iChanged);
	return 2;
}
// SetGrassKillBox(slot, x, y, z, halfx, halfy, halfz [, yaw [, trees]]) - slot 1 to 8: no grass blade whose root is inside
// the box is drawn (under a vehicle, say). The box is centred on x, y, z, reaches half its size each way and is turned by
// yaw degrees about Y, as an object's angle Y is; move it every frame to follow something. Cleared at level start and test
// game end. trees 1 (a blast, not a vehicle) also removes every tree whose trunk reaches into the box, for the rest of the
// level: they stay gone when the box moves or is cleared
int SetGrassKillBox(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 7) return 0;
	int iSlot = lua_tointeger(L, 1);
	if (iSlot < 1 || iSlot > GGGRASS_MAX_KILLBOXES) return 0;
	float fYaw = 0.0f;
	if (n >= 8) fYaw = lua_tonumber(L, 8);
	GGGrass_SetKillBox(iSlot - 1, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), fYaw);
	if (n >= 9 && lua_tointeger(L, 9) != 0) GGTrees::GGTrees_KillBox(lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), fYaw);
	return 0;
}
// ClearGrassKillBox([slot]) - the grass comes back in that slot's box, or in every box without a slot
int ClearGrassKillBox(lua_State* L)
{
	int n = LUA_GETTOP(L);
	int iSlot = 0;
	if (n >= 1) iSlot = lua_tointeger(L, 1);
	if (iSlot == 0)
		GGGrass_ClearKillBox(-1);
	else if (iSlot >= 1 && iSlot <= GGGRASS_MAX_KILLBOXES)
		GGGrass_ClearKillBox(iSlot - 1);
	return 0;
}
// Tree detail controls. Values set here override the graphics quality presets (and survive a quality change) until the
// editor's test game ends. An omitted or 0 argument keeps the current value.
// SetTreeDistance(fullDetailDist [, shadowDist]) - where trees switch from full detail to billboards, 750 to 20000 units
int SetTreeDistance(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float fDist = lua_tonumber(L, 1);
	float fShadowDist = (n >= 2) ? lua_tonumber(L, 2) : 0.0f;
	if (fDist > 0) fDist = min(max(fDist, 750.0f), 20000.0f);
	if (fShadowDist > 0) fShadowDist = min(max(fShadowDist, 750.0f), 20000.0f);
	GGTrees::GGTrees_SetLuaDistances(fDist, fShadowDist);
	return 0;
}
int GetTreeDistance(lua_State* L)
{
	lua_pushnumber(L, GGTrees::ggtrees_global_params.lod_dist);
	lua_pushnumber(L, GGTrees::ggtrees_global_params.lod_dist_shadow);
	return 2;
}
// SetTreeTransition(width [, shadowWidth]) - width of the band where billboards and full detail trees cross fade, 100 to 4000 units (default 500)
int SetTreeTransition(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float fWidth = lua_tonumber(L, 1);
	float fShadowWidth = (n >= 2) ? lua_tonumber(L, 2) : 0.0f;
	if (fWidth > 0) fWidth = min(max(fWidth, 100.0f), 4000.0f);
	if (fShadowWidth > 0) fShadowWidth = min(max(fShadowWidth, 100.0f), 4000.0f);
	GGTrees::GGTrees_SetLuaTransitions(fWidth, fShadowWidth);
	return 0;
}
int GetTreeTransition(lua_State* L)
{
	lua_pushnumber(L, GGTrees::ggtrees_global_params.lod_transition);
	lua_pushnumber(L, GGTrees::ggtrees_global_params.lod_transition_shadow);
	return 2;
}
// SetTreeShadowCascades(billboardCascades [, fullDetailCascades]) - how many sun shadow cascades (0 to 5, nearest first) draw
// tree shadows; a negative or omitted value keeps the current one
int SetTreeShadowCascades(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iBillboard = lua_tonumber(L, 1);
	int iFullDetail = (n >= 2) ? lua_tonumber(L, 2) : -1;
	if (iBillboard > 5) iBillboard = 5;
	if (iFullDetail > 5) iFullDetail = 5;
	GGTrees::GGTrees_SetLuaShadowCascades(iBillboard, iFullDetail);
	return 0;
}
int GetTreeShadowCascades(lua_State* L)
{
	lua_pushnumber(L, GGTrees::ggtrees_global_params.tree_shadow_range);
	lua_pushnumber(L, GGTrees::ggtrees_global_params.tree_shadow_range_high);
	return 2;
}
// RayTrees(x1, y1, z1, x2, y2, z2): the nearest tree trunk the segment meets, anywhere on the map (the physics sees only
// those round the camera). Returns 1 and the point and the trunk's normal there, or 0 and zeros. A trunk is as thick as
// the trees' physics collision (by species and scale) and reaches from the ground to the tree's top; canopies don't
// count, nor trees hidden under a building or removed by a blast. Only the tree chunks along the segment are walked
int RayTrees(lua_State* L)
{
	if (LUA_GETTOP(L) < 6) return 0;
	float fOut[6] = { 0, 0, 0, 0, 0, 0 };
	int iHit = GGTrees::GGTrees_RayCastTrunks(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tonumber(L, 6), fOut);
	lua_pushinteger(L, iHit);
	for (int i = 0; i < 6; i++) lua_pushnumber(L, fOut[i]);
	return 7;
}
// GetTreesNear(x, z, radius [, max]): the tree trunks within radius of x, z across (at any height), nearest first, as a
// table of { x, y, z, radius, type, name, top, crown, scale }: each trunk's base and its radius, as the trees' collision has it
// (by species and scale), its tree type and that type's name ("jungletree3a"), the tree's top, crown (where its crown starts:
// the lowest branch or leaf of its species' full detail mesh at its scale, the top for a species with none; below it only the
// bare trunk) and its own scale.
// At most max (64 unless set). Trees not drawn are left out: hidden under a building, removed by a blast, or all of them
// when trees are off. For a script's own collision test, such as a car driven by Lua
int GetTreesNear(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fZ = lua_tonumber(L, 2);
	float fRadius = lua_tonumber(L, 3);
	int iMax = (n >= 4) ? lua_tointeger(L, 4) : 64;
	GGTrees::GGTreePoint* pPoints = NULL;
	int iCount = 0;
	if (GGTrees::ggtrees_global_params.draw_enabled && fRadius > 0) iCount = GGTrees::GGTrees_GetClosest(fX, fZ, fRadius, &pPoints);

	// the nearest first
	std::vector<std::pair<float, int>> nearest;
	for (int i = 0; i < iCount; i++)
	{
		float fDX = pPoints[i].x - fX;
		float fDZ = pPoints[i].z - fZ;
		std::pair<float, int> tree(fDX * fDX + fDZ * fDZ, i);
		size_t k = nearest.size();
		nearest.push_back(tree);
		while (k > 0 && nearest[k - 1].first > tree.first) { nearest[k] = nearest[k - 1]; k--; }
		nearest[k] = tree;
	}

	int iTake = (int)nearest.size() < iMax ? (int)nearest.size() : iMax;
	if (iTake < 0) iTake = 0;
	lua_createtable(L, iTake, 0);
	for (int k = 0; k < iTake; k++)
	{
		GGTrees::GGTreePoint& tree = pPoints[nearest[k].second];
		lua_createtable(L, 0, 9);
		lua_pushnumber(L, tree.x); lua_setfield(L, -2, "x");
		lua_pushnumber(L, tree.y); lua_setfield(L, -2, "y");
		lua_pushnumber(L, tree.z); lua_setfield(L, -2, "z");
		lua_pushnumber(L, tree.scale * 0.5f); lua_setfield(L, -2, "radius");
		lua_pushinteger(L, tree.type); lua_setfield(L, -2, "type");
		char pName[64];
		GGTrees::GGTrees_GetTypeName(tree.type, pName, sizeof(pName));
		lua_pushstring(L, pName); lua_setfield(L, -2, "name");
		lua_pushnumber(L, tree.top); lua_setfield(L, -2, "top");
		lua_pushnumber(L, tree.crown); lua_setfield(L, -2, "crown");
		lua_pushnumber(L, tree.instanceScale); lua_setfield(L, -2, "scale");
		lua_rawseti(L, -2, k + 1);
	}
	if (pPoints) delete[] pPoints;
	return 1;
}

// GetGameQuality(): the graphics quality in force, 1 Low, 2 Medium, 3 High (the level's own settings), 4 Ultra
int GetGameQuality(lua_State* L)
{
	extern int quality_getapplied(void);
	lua_pushinteger(L, quality_getapplied() + 1);
	return 1;
}

namespace GGTerrain
{
	int GGTerrain_GetPagesNeeded();
	int GGTerrain_GetPagesActive();
	void GGTerrain_GetPageCounts( uint32_t* pReadBacks, uint32_t* pPagesMade );
}
// GetTerrainPageStats(): the terrain's virtual texture: pages wanted on screen that aren't made yet (a backlog, which grows
// while the ground shows lower detail), pages wanted on screen in all, then since the level started the read backs taken and
// the pages made (take their change over a second for rates)
int GetTerrainPageStats(lua_State* L)
{
	uint32_t uReadBacks = 0, uPagesMade = 0;
	GGTerrain::GGTerrain_GetPageCounts(&uReadBacks, &uPagesMade);
	lua_pushinteger(L, GGTerrain::GGTerrain_GetPagesNeeded());
	lua_pushinteger(L, GGTerrain::GGTerrain_GetPagesActive());
	lua_pushinteger(L, uReadBacks);
	lua_pushinteger(L, uPagesMade);
	return 4;
}

// GetTreeCounts(): the trees drawn at full detail this frame, and the trees in the full detail shadow list (both set by the
// tree distance and transition)
int GetTreeCounts(lua_State* L)
{
	lua_pushinteger(L, GGTrees::GGTrees_GetNumHighDetail());
	lua_pushinteger(L, GGTrees::GGTrees_GetNumHighDetailShadow());
	return 2;
}

// GetTreeTypeName(type): the name of a tree type (GetTreesNear's type, PhysicsRay's treetype), such as "jungletree3a"; ""
// for none
int GetTreeTypeName(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iType = (int)lua_tointeger(L, 1);
	char pName[64] = "";
	if (iType >= 0) GGTrees::GGTrees_GetTypeName((uint32_t)iType, pName, sizeof(pName));
	lua_pushstring(L, pName);
	return 1;
}

//PE: USE - SetLutTo("editors\\lut\\sephia.png")
//PE: USE - string = GetLut()
void Wicked_Update_Visuals(void* voidvisual);
void Wicked_Update_LUT(void* voidvisual);
int lua_get_lut(lua_State* L)
{
	// SetLutTo writes t.gamevisuals (in a test game only that, so the editor keeps its own LUT), so read it back from there
	std::string retstr = t.gamevisuals.ColorGradingLUT.Get();
	if(retstr.length() > 0)
		lua_pushstring(lua, retstr.c_str());
	else
		lua_pushstring(lua, "None");
	return 1;
}

int lua_set_lut(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int storee = t.e;
	cstr stores = t.s_s;
	if (n == 1)
	{
		const char* pStrPtr = lua_tostring(L, 1);
		if (pStrPtr)
			t.s_s = pStrPtr;
		else
			t.s_s = "";
	}
	//PE: Keep away from lutImages_s, so we dont need to load anything.
	//PE: Only use gamevisuals so when we go back it restore the original settings.
	if (t.s_s.Len() > 0)
	{
		if (FileExist(t.s_s.Get()))
		{
			t.gamevisuals.ColorGradingLUT = t.s_s;
			t.gamevisuals.bColorGrading = true;
		}
		else
		{
			//PE: None.
			t.gamevisuals.ColorGradingLUT = "None";
			t.gamevisuals.bColorGrading = false;
		}

		// if in standalone game, also adjust visuals so graphics settings screen does not revert these changes
		if ( t.game.gameisexe == 1 )
		{
			t.visuals.ColorGradingLUT = t.gamevisuals.ColorGradingLUT;
			t.visuals.bColorGrading = t.gamevisuals.bColorGrading;
		}

		// only the LUT: the full Wicked_Update_Visuals push also reset script-set exposure, fog, sun and ambience to the
		// level values and recreated the swap chain (vsync), which showed as a blip
		Wicked_Update_LUT(&t.gamevisuals);
		g_bLuaSetLUT = true;
	}

	t.e = storee;
	t.s_s = stores;
	return 0;
}

// 260 LUA Internal Commands (was in lua_loop_finish)
enum eInternalCommandNames
{
	enum_collisionon,
	enum_drownplayer,
	enum_finishlevel,
	enum_hideterrain,
	enum_jumptolevel,
	enum_lookatangle,
	enum_lookforward,
	enum_moveforward,
	enum_promptvideo,
	enum_promptimage,
	enum_promptlocal,
	enum_rotatelimbx,
	enum_rotatelimby,
	enum_rotatelimbz,
	enum_setfoggreen,
	enum_showterrain,
	enum_spawnifused,
	enum_textcenterx,
	enum_setservertimer,
	enum_setsoundvolume,
	enum_setsurfaceblue,
	enum_setterrainsize,
	enum_switchpageback,
	enum_unfreezeplayer,
	enum_activateifused,
	enum_addplayerpower,
	enum_loopnon3dsound,
	enum_musicsetlength,
	enum_musicsetvolume,
	enum_playnon3dsound,
	enum_setambiencered,
	enum_rotatetocamera,
	enum_rotatetoplayer,
	enum_promptduration,
	enum_prompttextsize,
	enum_resetpositionx,
	enum_resetpositiony,
	enum_resetpositionz,
	enum_resetrotationx,
	enum_resetrotationy,
	enum_resetrotationz,
	enum_setfogdistance,
	enum_setgamequality,
	enum_sethoverfactor,
	enum_setplayerlives,
	enum_setplayerpower,
	enum_hide,
	enum_show,
	enum_changeplayerweapon,
	enum_playcharactersound,
	enum_setcameraweaponfov,
	enum_setanimationframes,
	enum_removeplayerweapon,
	enum_setfreezepositionx,
	enum_setfreezepositiony,
	enum_setfreezepositionz,
	enum_setgamemusicvolume,
	enum_setgamesoundvolume,
	enum_setloadingresource,
	enum_setoptionlightrays,
	enum_setoptionocclusion,
	enum_setvegetationwidth,
	enum_removeplayerweapons,
	enum_getentityplrvisible,
	enum_levelfilenametoload,
	enum_replaceplayerweapon,
	enum_setfreezepositionax,
	enum_setfreezepositionay,
	enum_setfreezepositionaz,
	enum_setoptionreflection,
	enum_setoptionvegetation,
	enum_setplayerhealthcore,
	enum_setpostsaointensity,
	enum_setserverkillstowin,
	enum_setsurfaceintensity,
	enum_setsurfacesunfactor,
	enum_setvegetationheight,
	enum_stopparticleemitter,
	enum_lookattargetyoffset,
	enum_setanimation,
	enum_setactivated,
	enum_setconstrast,
	enum_setcamerafov,
	enum_collisionoff,
	enum_freezeplayer,
	enum_lookatplayer,
	enum_lookattarget,
	enum_movebackward,
	enum_mp_aimovetox,
	enum_mp_aimovetoz,
	enum_musicplaycue,
	enum_nameplateson,
	enum_resetlimbhit,
	enum_ragdollforce,
	enum_setforcelimb,
	enum_setplayerfov,
	enum_setnogravity,
	enum_setlimbindex,
	enum_setrotationx,
	enum_setrotationy,
	enum_setrotationz,
	enum_setpositionx,
	enum_setpositiony,
	enum_setpositionz,
	enum_setpostbloom,
	enum_switchscript,
	enum_moveup,
	enum_mp_aimovetoy,
	enum_panelx,
	enum_panely,
	enum_panelz,
	enum_prompt,
	enum_destroy,
	enum_dopanel,
	enum_panelx2,
	enum_panely2,
	enum_rotatex,
	enum_rotatey,
	enum_rotatez,
	enum_wingame,
	enum_textred,
	enum_texttxt,
	enum_freezeai,
	enum_hidehuds,
	enum_loadgame,
	enum_losegame,
	enum_savegame,
	enum_quitgame,
	enum_setskyto,
	enum_setsound,
	enum_showhuds,
	enum_textblue,
	enum_textsize,
	enum_collected,
	enum_hideimage,
	enum_hidewater,
	enum_leavegame,
	enum_loopsound,
	enum_musicload,
	enum_musicstop,
	enum_playsound,
	enum_playvideo,
	enum_setforcex,
	enum_setforcey,
	enum_setforcez,
	enum_setfogred,
	enum_stopvideo,
	enum_startgame,
	enum_showimage,
	enum_showwater,
	enum_stopsound,
	enum_textgreen,
	enum_checkpoint,
	enum_fireweapon,
	enum_loadimages,
	enum_hurtplayer,
	enum_mpgamemode,
	enum_playspeech,
	enum_resumegame,
	enum_setfogblue,
	enum_switchpage,
	enum_stopspeech,
	enum_starttimer,
	enum_unfreezeai,
	enum_disablemusicreset,
	enum_fireweaponinstant,
	enum_loopanimationfrom,
	enum_movewithanimation,
	enum_playanimationfrom,
	enum_setactivatedformp,
	enum_setcameradistance,
	enum_setanimationframe,
	enum_changeanimationframe,
	enum_setanimationspeed,
	enum_setcharactersound,
	enum_promptvideonoskip,
	enum_playsoundifsilent,
	enum_setglobalspecular,
	enum_setimagealignment,
	enum_setimagepositionx,
	enum_setimagepositiony,
	enum_setterrainlodnear,
	enum_transporttoifused,
	enum_setbrightness,
	enum_serverendplay,
	enum_activatemouse,
	enum_addplayerammo,
	enum_aimsmoothmode,
	enum_lookattargete,
	enum_loopanimation,
	enum_modulatespeed,
	enum_musicplaytime,
	enum_musicplayfade,
	enum_playanimation,
	enum_nameplatesoff,
	enum_refreshentity,
	enum_setfognearest,
	enum_setsoundspeed,
	enum_setsurfacered,
	enum_stopanimation,
	enum_triggerfadein,
	enum_deactivatemouse,
	enum_addplayerhealth,
	enum_addplayerweapon,
	enum_getentityinzone,
	enum_musicsetdefault,
	enum_setambienceblue,
	enum_playvideonoskip,
	enum_setfogintensity,
	enum_setentityhealth,
	enum_setlightvisible,
	enum_setsurfacegreen,
	enum_addplayerjetpack,
	enum_musicplayinstant,
	enum_musicplaytimecue,
	enum_musicsetfadetime,
	enum_musicsetinterval,
	enum_serverrespawnall,
	enum_setambiencegreen,
	enum_setanimationname,
	enum_promptlocalforvr,
	enum_setlockcharacter,
	enum_setoptionshadows,
	enum_setpostsaoradius,
	enum_setterrainlodfar,
	enum_setterrainlodmid,
	enum_scale,
	enum_spawn,
	enum_textx,
	enum_texty,
	enum_changeplayerweaponid,
	enum_setcharactersoundset,
	enum_setcharactertostrafe,
	enum_promptlocalforvrmode,
	enum_setambienceintensity,
	enum_setpostlightraydecay,
	enum_startparticleemitter,
	enum_charactercontrolarmed,
	enum_charactercontrollimbo,
	enum_charactercontrolstand,
	enum_setcharactertowalkrun,
	enum_setentityhealthsilent,
	enum_setpostlightraylength,
	enum_setpostmotiondistance,
	enum_setpostvignetteradius,
	enum_setvegetationquantity,
	enum_charactercontrolducked,
	enum_charactercontrolfidget,
	enum_charactercontrolmanual,
	enum_setpostmotionintensity,
	enum_setpostlightrayquality,
	enum_charactercontrolunarmed,
	enum_performlogicconnections,
	enum_setcamerazoompercentage,
	enum_setcharactervisiondelay,
	enum_rotatetoplayerwithoffset,
	enum_setpostvignetteintensity,
	enum_setpostlensflareintensity,
	enum_transporttofreezeposition,
	enum_setentityhealthwithdamage,
	enum_setpostdepthoffielddistance,
	enum_setpostdepthoffieldintensity,
	enum_performlogicconnectionsaskey,
	enum_setprioritytotransporter,
	enum_hidelimbs,
	enum_showlimbs,
	enum_performlogicconnectionnumber,
};

// NoParam commands:
//startgame lua_startgame(); }
//loadgame lua_loadgame(); }
//savegame lua_savegame(); }
//quitgame lua_quitgame(); }
//leavegame lua_leavegame(); }
//resumegame lua_resumegame(); }
//switchpageback lua_switchpageback(); }
//triggerfadein lua_triggerfadein(); }
//wingame lua_wingame(); }
//losegame lua_losegame(); }
// lowest entity id each command accepts, for the commands whose handlers index t.entityelement[t.e]
// (the one-argument integer path also copies non-entity values into t.e, so only these are checked)
#define LUAENTITY_NOTENTITYCOMMAND 0x7fffffff
int LuaEntityCommandMinID ( eInternalCommandNames eInternalCommandValue )
{
	switch ( eInternalCommandValue )
	{
		// -1 or 0 mean the damage has no source entity
		case enum_hurtplayer: case enum_drownplayer:
			return -1;
		// 0 means no entity
		case enum_checkpoint: case enum_jumptolevel: case enum_getentityplrvisible:
			return 0;
		case enum_collisionon: case enum_promptvideo: case enum_spawnifused: case enum_activateifused:
		case enum_loopnon3dsound: case enum_playnon3dsound: case enum_rotatetocamera: case enum_rotatetoplayer:
		case enum_hide: case enum_show: case enum_replaceplayerweapon: case enum_setactivated:
		case enum_collisionoff: case enum_resetlimbhit: case enum_ragdollforce: case enum_setforcelimb:
		case enum_setnogravity: case enum_setlimbindex: case enum_destroy: case enum_setsound:
		case enum_collected: case enum_loopsound: case enum_playsound: case enum_playvideo:
		case enum_stopvideo: case enum_stopsound: case enum_playspeech: case enum_starttimer:
		case enum_loopanimationfrom: case enum_movewithanimation: case enum_playanimationfrom: case enum_setactivatedformp:
		case enum_promptvideonoskip: case enum_playsoundifsilent: case enum_transporttoifused: case enum_addplayerammo:
		case enum_loopanimation: case enum_playanimation: case enum_refreshentity: case enum_stopanimation:
		case enum_addplayerhealth: case enum_addplayerweapon: case enum_getentityinzone: case enum_playvideonoskip:
		case enum_setentityhealth: case enum_setlightvisible: case enum_addplayerjetpack: case enum_spawn:
		case enum_setcharactersoundset: case enum_setentityhealthsilent: case enum_performlogicconnections: case enum_performlogicconnectionnumber:
		case enum_setentityhealthwithdamage: case enum_performlogicconnectionsaskey: case enum_setprioritytotransporter: case enum_hidelimbs:
		case enum_showlimbs: case enum_moveforward: case enum_rotatelimbx: case enum_rotatelimby:
		case enum_rotatelimbz: case enum_resetpositionx: case enum_resetpositiony: case enum_resetpositionz:
		case enum_resetrotationx: case enum_resetrotationy: case enum_resetrotationz: case enum_sethoverfactor:
		case enum_stopparticleemitter: case enum_movebackward: case enum_setrotationx: case enum_setrotationy:
		case enum_setrotationz: case enum_setpositionx: case enum_setpositiony: case enum_setpositionz:
		case enum_moveup: case enum_rotatex: case enum_rotatey: case enum_rotatez:
		case enum_setanimationframe: case enum_changeanimationframe: case enum_setanimationspeed: case enum_modulatespeed:
		case enum_scale: case enum_startparticleemitter: case enum_rotatetoplayerwithoffset: case enum_switchscript:
		case enum_setanimationname: case enum_setcharactersound: case enum_playcharactersound:
			return 1;
	}
	return LUAENTITY_NOTENTITYCOMMAND;
}

bool LuaEntityCommandValid ( lua_State* L, eInternalCommandNames eInternalCommandValue )
{
	int iMinID = LuaEntityCommandMinID ( eInternalCommandValue );
	if ( iMinID == LUAENTITY_NOTENTITYCOMMAND ) return true;
	return LuaEntityIDValid ( L, t.e, iMinID );
}

int int_core_sendmessagenone(lua_State* L, eInternalCommandNames eInternalCommandValue)
{
	int n = LUA_GETTOP(L);
	if (n > 0) return 0;
	int storee = t.e;
	int storev = t.v;
	switch (eInternalCommandValue)
	{
		case enum_startgame: lua_startgame(); break;
		case enum_loadgame: lua_loadgame(); break;
		case enum_savegame: lua_savegame(); break;
		case enum_quitgame: lua_quitgame(); break;
		case enum_leavegame: lua_leavegame(); break;
		case enum_resumegame: lua_resumegame(); break;
		case enum_switchpageback: lua_switchpageback(); break;
		case enum_triggerfadein: lua_triggerfadein(); break;
		case enum_wingame: lua_wingame(); break;
		case enum_losegame: lua_losegame(); break;
		case enum_musicstop: lua_musicstop(); break;
	}
}
int SendMessage_startgame(lua_State* L) { return int_core_sendmessagenone(L, enum_startgame); }
int SendMessage_loadgame(lua_State* L) { return int_core_sendmessagenone(L, enum_loadgame); }
int SendMessage_savegame(lua_State* L) { return int_core_sendmessagenone(L, enum_savegame); }
int SendMessage_quitgame(lua_State* L) { return int_core_sendmessagenone(L, enum_quitgame); }
int SendMessage_leavegame(lua_State* L) { return int_core_sendmessagenone(L, enum_leavegame); }
int SendMessage_resumegame(lua_State* L) { return int_core_sendmessagenone(L, enum_resumegame); }
int SendMessage_switchpageback(lua_State* L) { return int_core_sendmessagenone(L, enum_switchpageback); }
int SendMessage_triggerfadein(lua_State* L) { return int_core_sendmessagenone(L, enum_triggerfadein); }
int SendMessage_wingame(lua_State* L) { return int_core_sendmessagenone(L, enum_wingame); }
int SendMessage_losegame(lua_State* L) { return int_core_sendmessagenone(L, enum_losegame); }
int SendMessage_musicstop(lua_State* L) { return int_core_sendmessagenone(L, enum_musicstop); }
void addInternalFunctions_nones()
{
	lua_register(lua, "SendMessage_startgame", SendMessage_startgame);
	lua_register(lua, "SendMessage_loadgame", SendMessage_loadgame);
	lua_register(lua, "SendMessage_savegame", SendMessage_savegame);
	lua_register(lua, "SendMessage_quitgame", SendMessage_quitgame);
	lua_register(lua, "SendMessage_leavegame", SendMessage_leavegame);
	lua_register(lua, "SendMessage_resumegame", SendMessage_resumegame);
	lua_register(lua, "SendMessage_switchpageback", SendMessage_switchpageback);
	lua_register(lua, "SendMessage_triggerfadein", SendMessage_triggerfadein);
	lua_register(lua, "SendMessage_wingame", SendMessage_wingame);
	lua_register(lua, "SendMessage_losegame", SendMessage_losegame);
	lua_register(lua, "SendMessage_musicstop", SendMessage_musicstop);
}

// Internal Integer commands:
int int_core_sendmessagei(lua_State* L, eInternalCommandNames eInternalCommandValue)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int storee = t.e;
	int storev = t.v;
	if (n == 1)
	{
		t.v = lua_tonumber(L, 1);
		t.e = t.v; // old legacy code confused LuaMessageInt() and LuaMessageIndex(), assigning to t.e and t.v arbitarily!
	}
	if (n == 2)
	{
		t.e = lua_tonumber(L, 1);
		t.v = lua_tonumber(L, 2);
	}
	if (!LuaEntityCommandValid(L, eInternalCommandValue))
	{
		t.e = storee;
		t.v = storev;
		return 0;
	}
	switch (eInternalCommandValue)
	{
		case enum_collisionon: entity_lua_collisionon(); break;
		case enum_drownplayer: entity_lua_drownplayer(); break;
		case enum_finishlevel: lua_finishlevel(); break;
		case enum_hideterrain: lua_hideterrain(); break;
		case enum_promptvideo: entity_lua_playvideonoskip(1, 0); break;
		case enum_promptimage: lua_promptimage(); break;
		case enum_showterrain: lua_showterrain(); break;
		case enum_spawnifused: entity_lua_spawnifused(); break;
		case enum_textcenterx: t.tluaTextCenterX = 1; break;
		case enum_setservertimer: mp_setServerTimer(); break;
		case enum_setsoundvolume: entity_lua_setsoundvolume(); break;
		case enum_unfreezeplayer: lua_unfreezeplayer(); break;
		case enum_activateifused: entity_lua_activateifused(); break;
		case enum_addplayerpower: entity_lua_addplayerpower(); break;
		case enum_loopnon3dsound: entity_lua_loopnon3Dsound(); break;
		case enum_musicsetlength: t.m = t.e; lua_musicsetlength(); break;
		case enum_musicsetvolume: lua_musicsetvolume(); break;
		case enum_playnon3dsound: entity_lua_playnon3Dsound(); break;
		case enum_rotatetocamera: entity_lua_rotatetocamera(); break;
		case enum_rotatetoplayer: entity_lua_rotatetoplayer(); break;
		case enum_prompttextsize: lua_prompttextsize(); break;
		case enum_setgamequality: lua_setgamequality(); break;
		case enum_setplayerhealthcore: lua_setplayerhealthcore(); break;
		case enum_setplayerlives: lua_setplayerlives(); break;
		case enum_setplayerpower: entity_lua_setplayerpower(); break;
		case enum_hide: entity_lua_hide(); break;
		case enum_show: entity_lua_show(); break;
		case enum_setanimationframes: entity_lua_setanimationframes(); break;
		case enum_removeplayerweapon: lua_removeplayerweapon(); break;
		case enum_setgamemusicvolume: lua_setgamemusicvolume(); break;
		case enum_setgamesoundvolume: lua_setgamesoundvolume(); break;
		case enum_setloadingresource: lua_setloadingresource(); break;
		case enum_getentityplrvisible: entity_lua_getentityplrvisible(); break;
		case enum_replaceplayerweapon: entity_lua_replaceplayerweapon(); break;
		case enum_setserverkillstowin: mp_setServerKillsToWin(); break;
		case enum_removeplayerweapons: lua_removeplayerweapons(); break;
		case enum_setanimation: entity_lua_setanimation(); break;
		case enum_setactivated: entity_lua_setactivated(); break;
		case enum_collisionoff: entity_lua_collisionoff(); break;
		case enum_freezeplayer: lua_freezeplayer(); break;
		case enum_musicplaycue: t.m = t.e; lua_musicplaycue(); break;
		case enum_nameplateson: g.mp.nameplatesOff = 0; break;
		case enum_resetlimbhit: entity_lua_resetlimbhit(); break;
		case enum_ragdollforce: entity_lua_ragdollforce(); break;
		case enum_setforcelimb: entity_lua_setforcelimb(); break;
		case enum_setforcex: entity_lua_setforcex(); break;
		case enum_setforcey: entity_lua_setforcey(); break;
		case enum_setforcez: entity_lua_setforcez(); break;
		case enum_setplayerfov: lua_setplayerfov(); break;
		case enum_setnogravity: entity_lua_set_gravity(); break;
		case enum_setlimbindex: entity_lua_setlimbindex(); break;
		case enum_destroy: entity_lua_destroy(); break;
		case enum_dopanel: lua_panel(); break;
		case enum_textred: g.mp.steamColorRed = t.v; g.mp.steamDoColorText = 1; break;
		case enum_freezeai: lua_freezeai(); break;
		case enum_hidehuds: lua_hidehuds(); break;
		case enum_setsound: entity_lua_setsound(); break;
		case enum_showhuds: lua_showhuds(); break;
		case enum_textblue: g.mp.steamColorBlue = t.v; break;
		case enum_textsize: t.luaText.size = t.v; break;
		case enum_collected: entity_lua_collected(); break;
		case enum_hideimage: lua_hideimage(); break;
		case enum_hidewater: lua_hidewater(); break;
		case enum_loopsound: entity_lua_loopsound(); break;
		case enum_playsound: entity_lua_playsound(); break;
		case enum_playvideo: entity_lua_playvideonoskip(0, 0); break;
		case enum_stopvideo: entity_lua_stopvideo(); break;
		case enum_showimage: lua_showimage(); break;
		case enum_showwater: lua_showwater(); break;
		case enum_stopsound: entity_lua_stopsound(); break;
		case enum_textgreen: g.mp.steamColorGreen = t.v; break;
		case enum_checkpoint: entity_lua_checkpoint(); break;
		case enum_fireweapon: entity_lua_fireweapon(); break;
		case enum_hurtplayer: entity_lua_hurtplayer(); break;
		case enum_mpgamemode: mp_serverSetLuaGameMode(); break;
		case enum_playspeech: entity_lua_playspeech(); break;
		case enum_stopspeech: entity_lua_stopspeech(); break;
		case enum_starttimer: entity_lua_starttimer(); break;
		case enum_unfreezeai: lua_unfreezeai(); break;
		case enum_disablemusicreset: lua_disablemusicreset(); break;
		case enum_fireweaponinstant: entity_lua_fireweapon(true); break;
		case enum_loopanimationfrom: entity_lua_loopanimationfrom(); break;
		case enum_movewithanimation: entity_lua_movewithanimation(); break;
		case enum_playanimationfrom: entity_lua_playanimationfrom(); break;
		case enum_setactivatedformp: entity_lua_setactivatedformp(); break;
		case enum_promptvideonoskip: entity_lua_playvideonoskip(1, 1); break;
		case enum_playsoundifsilent: entity_lua_playsoundifsilent(); break;
		case enum_setimagealignment: lua_setimagealignment(); break;
		case enum_transporttoifused: entity_lua_transporttoifused(); break;
		case enum_serverendplay: mp_serverEndPlay(); break;
		case enum_activatemouse: lua_activatemouse(); break;
		case enum_addplayerammo: entity_lua_addplayerammo(); break;
		case enum_loopanimation: entity_lua_loopanimation(); break;
		case enum_musicplaytime: t.m = t.e; lua_musicplaytime(); break;
		case enum_musicplayfade: t.m = t.e; lua_musicplayfade(); lua_musicplayfade(); break;
		case enum_playanimation: entity_lua_playanimation(); break;
		case enum_nameplatesoff: g.mp.nameplatesOff = 1; break;
		case enum_refreshentity: entity_lua_refreshentity(); break;
		case enum_setsoundspeed: entity_lua_setsoundspeed(); break;
		case enum_stopanimation: entity_lua_stopanimation(); break;
		case enum_deactivatemouse: lua_deactivatemouse(); break;
		case enum_addplayerhealth: entity_lua_addplayerhealth(); break;
		case enum_addplayerweapon: entity_lua_addplayerweapon(); break;
		case enum_getentityinzone: entity_lua_getentityinzone(0); break;
		case enum_musicsetdefault: t.m = t.e; lua_musicsetdefault(); break;
		case enum_playvideonoskip: entity_lua_playvideonoskip(0, 1); break;
		case enum_setentityhealth: entity_lua_setentityhealth(); break;
		case enum_setlightvisible: entity_lua_set_light_visible(); break;
		case enum_addplayerjetpack: entity_lua_addplayerjetpack(); break;
		case enum_musicplayinstant: t.m = t.e; lua_musicplayinstant(); lua_musicplayinstant(); break;
		case enum_musicplaytimecue: t.m = t.e; lua_musicplaytimecue(); break;
		case enum_musicsetfadetime: t.m = t.e; lua_musicsetfadetime(); break;
		case enum_musicsetinterval: t.m = t.e; lua_musicsetinterval(); break;
		case enum_serverrespawnall: mp_serverRespawnAll(); break;
		case enum_setlockcharacter: entity_lua_setlockcharacter(); break;
		case enum_spawn: entity_lua_spawn(); break;
		case enum_changeplayerweaponid: entity_lua_changeplayerweaponid(); break;
		case enum_setcharactersoundset: character_soundset(); break;
		case enum_setcharactertostrafe: entity_lua_setcharactertostrafe(); break;
		case enum_charactercontrolarmed: entity_lua_charactercontrolarmed(); break;
		case enum_charactercontrollimbo: entity_lua_charactercontrollimbo(); break;
		case enum_charactercontrolstand: entity_lua_charactercontrolstand(); break;
		case enum_setcharactertowalkrun: entity_lua_setcharactertowalkrun(); break;
		case enum_setentityhealthsilent: entity_lua_setentityhealthsilent(); break;
		case enum_charactercontrolducked: entity_lua_charactercontrolducked(); break;
		case enum_charactercontrolfidget: entity_lua_charactercontrolfidget(); break;
		case enum_charactercontrolmanual: entity_lua_charactercontrolmanual(); break;
		case enum_charactercontrolunarmed: entity_lua_charactercontrolunarmed(); break;
		case enum_performlogicconnections: entity_lua_performlogicconnections(); break;
		case enum_performlogicconnectionnumber: entity_lua_performlogicconnectionnumber(); break;
		case enum_setcharactervisiondelay: entity_lua_setcharactervisiondelay(); break;
		case enum_transporttofreezeposition: lua_transporttofreezeposition(); break;
		case enum_setentityhealthwithdamage: entity_lua_setentityhealthwithdamage(); break;
		case enum_performlogicconnectionsaskey: entity_lua_performlogicconnectionsaskey(); break;
		case enum_setprioritytotransporter : lua_setprioritytotransporter(); break;
		case enum_hidelimbs: entity_lua_hidelimbs(); break;
		case enum_showlimbs: entity_lua_showlimbs(); break;
	}
	t.e = storee;
	t.v = storev;
	return 0;
}
int SendMessageI_collisionon(lua_State* L) { return int_core_sendmessagei(L, enum_collisionon); }
int SendMessageI_drownplayer(lua_State* L) { return int_core_sendmessagei(L, enum_drownplayer); }
int SendMessageI_finishlevel(lua_State* L) { return int_core_sendmessagei(L, enum_finishlevel); }
int SendMessageI_hideterrain(lua_State* L) { return int_core_sendmessagei(L, enum_hideterrain); }
int SendMessageI_promptvideo(lua_State* L) { return int_core_sendmessagei(L, enum_promptvideo); }
int SendMessageI_promptimage(lua_State* L) { return int_core_sendmessagei(L, enum_promptimage); }
int SendMessageI_showterrain(lua_State* L) { return int_core_sendmessagei(L, enum_showterrain); }
int SendMessageI_spawnifused(lua_State* L) { return int_core_sendmessagei(L, enum_spawnifused); }
int SendMessageI_textcenterx(lua_State* L) { return int_core_sendmessagei(L, enum_textcenterx); }
int SendMessageI_setservertimer(lua_State* L) { return int_core_sendmessagei(L, enum_setservertimer); }
int SendMessageI_setsoundvolume(lua_State* L) { return int_core_sendmessagei(L, enum_setsoundvolume); }
int SendMessageI_unfreezeplayer(lua_State* L) { return int_core_sendmessagei(L, enum_unfreezeplayer); }
int SendMessageI_activateifused(lua_State* L) { return int_core_sendmessagei(L, enum_activateifused); }
int SendMessageI_addplayerpower(lua_State* L) { return int_core_sendmessagei(L, enum_addplayerpower); }
int SendMessageI_loopnon3dsound(lua_State* L) { return int_core_sendmessagei(L, enum_loopnon3dsound); }
int SendMessageI_musicsetlength(lua_State* L) { return int_core_sendmessagei(L, enum_musicsetlength); }
int SendMessageI_musicsetvolume(lua_State* L) { return int_core_sendmessagei(L, enum_musicsetvolume); }
int SendMessageI_playnon3dsound(lua_State* L) { return int_core_sendmessagei(L, enum_playnon3dsound); }
int SendMessageI_rotatetocamera(lua_State* L) { return int_core_sendmessagei(L, enum_rotatetocamera); }
int SendMessageI_rotatetoplayer(lua_State* L) { return int_core_sendmessagei(L, enum_rotatetoplayer); }
int SendMessageI_prompttextsize(lua_State* L) { return int_core_sendmessagei(L, enum_prompttextsize); }
int SendMessageI_setgamequality(lua_State* L) { return int_core_sendmessagei(L, enum_setgamequality); }
int SendMessageI_setplayerhealthcore(lua_State* L) { return int_core_sendmessagei(L, enum_setplayerhealthcore); }
int SendMessageI_setplayerlives(lua_State* L) { return int_core_sendmessagei(L, enum_setplayerlives); }
int SendMessageI_setplayerpower(lua_State* L) { return int_core_sendmessagei(L, enum_setplayerpower); }
int SendMessageI_hide(lua_State* L) { return int_core_sendmessagei(L, enum_hide); }
int SendMessageI_show(lua_State* L) { return int_core_sendmessagei(L, enum_show); }
int SendMessageI_hidelimbs(lua_State* L) { return int_core_sendmessagei(L, enum_hidelimbs); }
int SendMessageI_showlimbs(lua_State* L) { return int_core_sendmessagei(L, enum_showlimbs); }
int SendMessageI_setanimationframes(lua_State* L) { return int_core_sendmessagei(L, enum_setanimationframes); }
int SendMessageI_removeplayerweapon(lua_State* L) { return int_core_sendmessagei(L, enum_removeplayerweapon); }
int SendMessageI_setgamemusicvolume(lua_State* L) { return int_core_sendmessagei(L, enum_setgamemusicvolume); }
int SendMessageI_setgamesoundvolume(lua_State* L) { return int_core_sendmessagei(L, enum_setgamesoundvolume); }
int SendMessageI_setloadingresource(lua_State* L) { return int_core_sendmessagei(L, enum_setloadingresource); }
int SendMessageI_getentityplrvisible(lua_State* L) { return int_core_sendmessagei(L, enum_getentityplrvisible); }
int SendMessageI_replaceplayerweapon(lua_State* L) { return int_core_sendmessagei(L, enum_replaceplayerweapon); }
int SendMessageI_setserverkillstowin(lua_State* L) { return int_core_sendmessagei(L, enum_setserverkillstowin); }
int SendMessageI_removeplayerweapons(lua_State* L) { return int_core_sendmessagei(L, enum_removeplayerweapons); }
int SendMessageI_setanimation(lua_State* L) { return int_core_sendmessagei(L, enum_setanimation); }
int SendMessageI_setactivated(lua_State* L) { return int_core_sendmessagei(L, enum_setactivated); }
int SendMessageI_collisionoff(lua_State* L) { return int_core_sendmessagei(L, enum_collisionoff); }
int SendMessageI_freezeplayer(lua_State* L) { return int_core_sendmessagei(L, enum_freezeplayer); }
int SendMessageI_musicplaycue(lua_State* L) { return int_core_sendmessagei(L, enum_musicplaycue); }
int SendMessageI_nameplateson(lua_State* L) { return int_core_sendmessagei(L, enum_nameplateson); }
int SendMessageI_resetlimbhit(lua_State* L) { return int_core_sendmessagei(L, enum_resetlimbhit); }
int SendMessageI_ragdollforce(lua_State* L) { return int_core_sendmessagei(L, enum_ragdollforce); }
int SendMessageI_setforcelimb(lua_State* L) { return int_core_sendmessagei(L, enum_setforcelimb); }
int SendMessageI_setforcex(lua_State* L) { return int_core_sendmessagei(L, enum_setforcex); }
int SendMessageI_setforcey(lua_State* L) { return int_core_sendmessagei(L, enum_setforcey); }
int SendMessageI_setforcez(lua_State* L) { return int_core_sendmessagei(L, enum_setforcez); }
int SendMessageI_setplayerfov(lua_State* L) { return int_core_sendmessagei(L, enum_setplayerfov); }
int SendMessageI_setnogravity(lua_State* L) { return int_core_sendmessagei(L, enum_setnogravity); }
int SendMessageI_setlimbindex(lua_State* L) { return int_core_sendmessagei(L, enum_setlimbindex); }
int SendMessageI_destroy(lua_State* L) { return int_core_sendmessagei(L, enum_destroy); }
int SendMessageI_dopanel(lua_State* L) { return int_core_sendmessagei(L, enum_dopanel); }
int SendMessageI_textred(lua_State* L) { return int_core_sendmessagei(L, enum_textred); }
int SendMessageI_freezeai(lua_State* L) { return int_core_sendmessagei(L, enum_freezeai); }
int SendMessageI_hidehuds(lua_State* L) { return int_core_sendmessagei(L, enum_hidehuds); }
int SendMessageI_setsound(lua_State* L) { return int_core_sendmessagei(L, enum_setsound); }
int SendMessageI_showhuds(lua_State* L) { return int_core_sendmessagei(L, enum_showhuds); }
int SendMessageI_textblue(lua_State* L) { return int_core_sendmessagei(L, enum_textblue); }
int SendMessageI_textsize(lua_State* L) { return int_core_sendmessagei(L, enum_textsize); }
int SendMessageI_collected(lua_State* L) { return int_core_sendmessagei(L, enum_collected); }
int SendMessageI_hideimage(lua_State* L) { return int_core_sendmessagei(L, enum_hideimage); }
int SendMessageI_hidewater(lua_State* L) { return int_core_sendmessagei(L, enum_hidewater); }
int SendMessageI_loopsound(lua_State* L) { return int_core_sendmessagei(L, enum_loopsound); }
int SendMessageI_playsound(lua_State* L) { return int_core_sendmessagei(L, enum_playsound); }
int SendMessageI_playvideo(lua_State* L) { return int_core_sendmessagei(L, enum_playvideo); }
int SendMessageI_stopvideo(lua_State* L) { return int_core_sendmessagei(L, enum_stopvideo); }
int SendMessageI_showimage(lua_State* L) { return int_core_sendmessagei(L, enum_showimage); }
int SendMessageI_showwater(lua_State* L) { return int_core_sendmessagei(L, enum_showwater); }
int SendMessageI_stopsound(lua_State* L) { return int_core_sendmessagei(L, enum_stopsound); }
int SendMessageI_textgreen(lua_State* L) { return int_core_sendmessagei(L, enum_textgreen); }
int SendMessageI_checkpoint(lua_State* L) { return int_core_sendmessagei(L, enum_checkpoint); }
int SendMessageI_fireweapon(lua_State* L) { return int_core_sendmessagei(L, enum_fireweapon); }
int SendMessageI_hurtplayer(lua_State* L) { return int_core_sendmessagei(L, enum_hurtplayer); }
int SendMessageI_mpgamemode(lua_State* L) { return int_core_sendmessagei(L, enum_mpgamemode); }
int SendMessageI_playspeech(lua_State* L) { return int_core_sendmessagei(L, enum_playspeech); }
int SendMessageI_stopspeech(lua_State* L) { return int_core_sendmessagei(L, enum_stopspeech); }
int SendMessageI_starttimer(lua_State* L) { return int_core_sendmessagei(L, enum_starttimer); }
int SendMessageI_unfreezeai(lua_State* L) { return int_core_sendmessagei(L, enum_unfreezeai); }
int SendMessageI_disablemusicreset(lua_State* L) { return int_core_sendmessagei(L, enum_disablemusicreset); }
int SendMessageI_fireweaponinstant(lua_State* L) { return int_core_sendmessagei(L, enum_fireweaponinstant); }
int SendMessageI_loopanimationfrom(lua_State* L) { return int_core_sendmessagei(L, enum_loopanimationfrom); }
int SendMessageI_movewithanimation(lua_State* L) { return int_core_sendmessagei(L, enum_movewithanimation); }
int SendMessageI_playanimationfrom(lua_State* L) { return int_core_sendmessagei(L, enum_playanimationfrom); }
int SendMessageI_setactivatedformp(lua_State* L) { return int_core_sendmessagei(L, enum_setactivatedformp); }
int SendMessageI_promptvideonoskip(lua_State* L) { return int_core_sendmessagei(L, enum_promptvideonoskip); }
int SendMessageI_playsoundifsilent(lua_State* L) { return int_core_sendmessagei(L, enum_playsoundifsilent); }
int SendMessageI_setimagealignment(lua_State* L) { return int_core_sendmessagei(L, enum_setimagealignment); }
int SendMessageI_transporttoifused(lua_State* L) { return int_core_sendmessagei(L, enum_transporttoifused); }
int SendMessageI_serverendplay(lua_State* L) { return int_core_sendmessagei(L, enum_serverendplay); }
int SendMessageI_activatemouse(lua_State* L) { return int_core_sendmessagei(L, enum_activatemouse); }
int SendMessageI_addplayerammo(lua_State* L) { return int_core_sendmessagei(L, enum_addplayerammo); }
int SendMessageI_loopanimation(lua_State* L) { return int_core_sendmessagei(L, enum_loopanimation); }
int SendMessageI_musicplaytime(lua_State* L) { return int_core_sendmessagei(L, enum_musicplaytime); }
int SendMessageI_musicplayfade(lua_State* L) { return int_core_sendmessagei(L, enum_musicplayfade); }
int SendMessageI_playanimation(lua_State* L) { return int_core_sendmessagei(L, enum_playanimation); }
int SendMessageI_nameplatesoff(lua_State* L) { return int_core_sendmessagei(L, enum_nameplatesoff); }
int SendMessageI_refreshentity(lua_State* L) { return int_core_sendmessagei(L, enum_refreshentity); }
int SendMessageI_setsoundspeed(lua_State* L) { return int_core_sendmessagei(L, enum_setsoundspeed); }
int SendMessageI_stopanimation(lua_State* L) { return int_core_sendmessagei(L, enum_stopanimation); }
int SendMessageI_deactivatemouse(lua_State* L) { return int_core_sendmessagei(L, enum_deactivatemouse); }
int SendMessageI_addplayerhealth(lua_State* L) { return int_core_sendmessagei(L, enum_addplayerhealth); }
int SendMessageI_addplayerweapon(lua_State* L) { return int_core_sendmessagei(L, enum_addplayerweapon); }
int SendMessageI_getentityinzone(lua_State* L) { return int_core_sendmessagei(L, enum_getentityinzone); }
int SendMessageI_musicsetdefault(lua_State* L) { return int_core_sendmessagei(L, enum_musicsetdefault); }
int SendMessageI_playvideonoskip(lua_State* L) { return int_core_sendmessagei(L, enum_playvideonoskip); }
int SendMessageI_setentityhealth(lua_State* L) { return int_core_sendmessagei(L, enum_setentityhealth); }
int SendMessageI_setlightvisible(lua_State* L) { return int_core_sendmessagei(L, enum_setlightvisible); }
int SendMessageI_addplayerjetpack(lua_State* L) { return int_core_sendmessagei(L, enum_addplayerjetpack); }
int SendMessageI_musicplayinstant(lua_State* L) { return int_core_sendmessagei(L, enum_musicplayinstant); }
int SendMessageI_musicplaytimecue(lua_State* L) { return int_core_sendmessagei(L, enum_musicplaytimecue); }
int SendMessageI_musicsetfadetime(lua_State* L) { return int_core_sendmessagei(L, enum_musicsetfadetime); }
int SendMessageI_musicsetinterval(lua_State* L) { return int_core_sendmessagei(L, enum_musicsetinterval); }
int SendMessageI_serverrespawnall(lua_State* L) { return int_core_sendmessagei(L, enum_serverrespawnall); }
int SendMessageI_setlockcharacter(lua_State* L) { return int_core_sendmessagei(L, enum_setlockcharacter); }
int SendMessageI_spawn(lua_State* L) { return int_core_sendmessagei(L, enum_spawn); }
int SendMessageI_changeplayerweaponid(lua_State* L) { return int_core_sendmessagei(L, enum_changeplayerweaponid); }
int SendMessageI_setcharactersoundset(lua_State* L) { return int_core_sendmessagei(L, enum_setcharactersoundset); }
int SendMessageI_setcharactertostrafe(lua_State* L) { return int_core_sendmessagei(L, enum_setcharactertostrafe); }
int SendMessageI_charactercontrolarmed(lua_State* L) { return int_core_sendmessagei(L, enum_charactercontrolarmed); }
int SendMessageI_charactercontrollimbo(lua_State* L) { return int_core_sendmessagei(L, enum_charactercontrollimbo); }
int SendMessageI_charactercontrolstand(lua_State* L) { return int_core_sendmessagei(L, enum_charactercontrolstand); }
int SendMessageI_setcharactertowalkrun(lua_State* L) { return int_core_sendmessagei(L, enum_setcharactertowalkrun); }
int SendMessageI_setentityhealthsilent(lua_State* L) { return int_core_sendmessagei(L, enum_setentityhealthsilent); }
int SendMessageI_charactercontrolducked(lua_State* L) { return int_core_sendmessagei(L, enum_charactercontrolducked); }
int SendMessageI_charactercontrolfidget(lua_State* L) { return int_core_sendmessagei(L, enum_charactercontrolfidget); }
int SendMessageI_charactercontrolmanual(lua_State* L) { return int_core_sendmessagei(L, enum_charactercontrolmanual); }
int SendMessageI_charactercontrolunarmed(lua_State* L) { return int_core_sendmessagei(L, enum_charactercontrolunarmed); }
int SendMessageI_performlogicconnections(lua_State* L) { return int_core_sendmessagei(L, enum_performlogicconnections); }
int SendMessageI_performlogicconnectionnumber(lua_State* L) { return int_core_sendmessagei(L, enum_performlogicconnectionnumber); }
int SendMessageI_setcharactervisiondelay(lua_State* L) { return int_core_sendmessagei(L, enum_setcharactervisiondelay); }
int SendMessageI_transporttofreezeposition(lua_State* L) { return int_core_sendmessagei(L, enum_transporttofreezeposition); }
int SendMessageI_setentityhealthwithdamage(lua_State* L) { return int_core_sendmessagei(L, enum_setentityhealthwithdamage); }
int SendMessageI_performlogicconnectionsaskey(lua_State* L) { return int_core_sendmessagei(L, enum_performlogicconnectionsaskey); }
int SendMessageI_setprioritytotransporter(lua_State* L) { return int_core_sendmessagei(L, enum_setprioritytotransporter); }
void addInternalFunctions_integer()
{
	lua_register(lua, "SendMessageI_collisionon", SendMessageI_collisionon);
	lua_register(lua, "SendMessageI_drownplayer", SendMessageI_drownplayer);
	lua_register(lua, "SendMessageI_finishlevel", SendMessageI_finishlevel);
	lua_register(lua, "SendMessageI_hideterrain", SendMessageI_hideterrain);
	lua_register(lua, "SendMessageI_promptvideo", SendMessageI_promptvideo);
	lua_register(lua, "SendMessageI_promptimage", SendMessageI_promptimage);
	lua_register(lua, "SendMessageI_showterrain", SendMessageI_showterrain);
	lua_register(lua, "SendMessageI_spawnifused", SendMessageI_spawnifused);
	lua_register(lua, "SendMessageI_textcenterx", SendMessageI_textcenterx);
	lua_register(lua, "SendMessageI_setservertimer", SendMessageI_setservertimer);
	lua_register(lua, "SendMessageI_setsoundvolume", SendMessageI_setsoundvolume);
	lua_register(lua, "SendMessageI_unfreezeplayer", SendMessageI_unfreezeplayer);
	lua_register(lua, "SendMessageI_activateifused", SendMessageI_activateifused);
	lua_register(lua, "SendMessageI_addplayerpower", SendMessageI_addplayerpower);
	lua_register(lua, "SendMessageI_loopnon3dsound", SendMessageI_loopnon3dsound);
	lua_register(lua, "SendMessageI_musicsetlength", SendMessageI_musicsetlength);
	lua_register(lua, "SendMessageI_musicsetvolume", SendMessageI_musicsetvolume);
	lua_register(lua, "SendMessageI_playnon3dsound", SendMessageI_playnon3dsound);
	lua_register(lua, "SendMessageI_rotatetocamera", SendMessageI_rotatetocamera);
	lua_register(lua, "SendMessageI_rotatetoplayer", SendMessageI_rotatetoplayer);
	lua_register(lua, "SendMessageI_prompttextsize", SendMessageI_prompttextsize);
	lua_register(lua, "SendMessageI_setgamequality", SendMessageI_setgamequality);
	lua_register(lua, "SendMessageI_setplayerhealthcore", SendMessageI_setplayerhealthcore);
	lua_register(lua, "SendMessageI_setplayerlives", SendMessageI_setplayerlives);
	lua_register(lua, "SendMessageI_setplayerpower", SendMessageI_setplayerpower);
	lua_register(lua, "SendMessageI_hide", SendMessageI_hide);
	lua_register(lua, "SendMessageI_show", SendMessageI_show);
	lua_register(lua, "SendMessageI_hidelimbs", SendMessageI_hidelimbs);
	lua_register(lua, "SendMessageI_showlimbs", SendMessageI_showlimbs);
	lua_register(lua, "SendMessageI_setanimationframes", SendMessageI_setanimationframes);
	lua_register(lua, "SendMessageI_removeplayerweapon", SendMessageI_removeplayerweapon);
	lua_register(lua, "SendMessageI_setgamemusicvolume", SendMessageI_setgamemusicvolume);
	lua_register(lua, "SendMessageI_setgamesoundvolume", SendMessageI_setgamesoundvolume);
	lua_register(lua, "SendMessageI_setloadingresource", SendMessageI_setloadingresource);
	lua_register(lua, "SendMessageI_getentityplrvisible", SendMessageI_getentityplrvisible);
	lua_register(lua, "SendMessageI_replaceplayerweapon", SendMessageI_replaceplayerweapon);
	lua_register(lua, "SendMessageI_setserverkillstowin", SendMessageI_setserverkillstowin);
	lua_register(lua, "SendMessageI_removeplayerweapons", SendMessageI_removeplayerweapons);
	lua_register(lua, "SendMessageI_setanimation", SendMessageI_setanimation);
	lua_register(lua, "SendMessageI_setactivated", SendMessageI_setactivated);
	lua_register(lua, "SendMessageI_collisionoff", SendMessageI_collisionoff);
	lua_register(lua, "SendMessageI_freezeplayer", SendMessageI_freezeplayer);
	lua_register(lua, "SendMessageI_musicplaycue", SendMessageI_musicplaycue);
	lua_register(lua, "SendMessageI_nameplateson", SendMessageI_nameplateson);
	lua_register(lua, "SendMessageI_resetlimbhit", SendMessageI_resetlimbhit);
	lua_register(lua, "SendMessageI_ragdollforce", SendMessageI_ragdollforce);
	lua_register(lua, "SendMessageI_setforcelimb", SendMessageI_setforcelimb);
	lua_register(lua, "SendMessageI_setforcex", SendMessageI_setforcex);
	lua_register(lua, "SendMessageI_setforcey", SendMessageI_setforcey);
	lua_register(lua, "SendMessageI_setforcez", SendMessageI_setforcez);
	lua_register(lua, "SendMessageI_setplayerfov", SendMessageI_setplayerfov);
	lua_register(lua, "SendMessageI_setnogravity", SendMessageI_setnogravity);
	lua_register(lua, "SendMessageI_setlimbindex", SendMessageI_setlimbindex);
	lua_register(lua, "SendMessageI_destroy", SendMessageI_destroy);
	lua_register(lua, "SendMessageI_dopanel", SendMessageI_dopanel);
	lua_register(lua, "SendMessageI_textred", SendMessageI_textred);
	lua_register(lua, "SendMessageI_freezeai", SendMessageI_freezeai);
	lua_register(lua, "SendMessageI_hidehuds", SendMessageI_hidehuds);
	lua_register(lua, "SendMessageI_setsound", SendMessageI_setsound);
	lua_register(lua, "SendMessageI_showhuds", SendMessageI_showhuds);
	lua_register(lua, "SendMessageI_textblue", SendMessageI_textblue);
	lua_register(lua, "SendMessageI_textsize", SendMessageI_textsize);
	lua_register(lua, "SendMessageI_collected", SendMessageI_collected);
	lua_register(lua, "SendMessageI_hideimage", SendMessageI_hideimage);
	lua_register(lua, "SendMessageI_hidewater", SendMessageI_hidewater);
	lua_register(lua, "SendMessageI_loopsound", SendMessageI_loopsound);
	lua_register(lua, "SendMessageI_playsound", SendMessageI_playsound);
	lua_register(lua, "SendMessageI_playvideo", SendMessageI_playvideo);
	lua_register(lua, "SendMessageI_stopvideo", SendMessageI_stopvideo);
	lua_register(lua, "SendMessageI_showimage", SendMessageI_showimage);
	lua_register(lua, "SendMessageI_showwater", SendMessageI_showwater);
	lua_register(lua, "SendMessageI_stopsound", SendMessageI_stopsound);
	lua_register(lua, "SendMessageI_textgreen", SendMessageI_textgreen);
	lua_register(lua, "SendMessageI_checkpoint", SendMessageI_checkpoint);
	lua_register(lua, "SendMessageI_fireweapon", SendMessageI_fireweapon);
	lua_register(lua, "SendMessageI_hurtplayer", SendMessageI_hurtplayer);
	lua_register(lua, "SendMessageI_mpgamemode", SendMessageI_mpgamemode);
	lua_register(lua, "SendMessageI_playspeech", SendMessageI_playspeech);
	lua_register(lua, "SendMessageI_stopspeech", SendMessageI_stopspeech);
	lua_register(lua, "SendMessageI_starttimer", SendMessageI_starttimer);
	lua_register(lua, "SendMessageI_unfreezeai", SendMessageI_unfreezeai);
	lua_register(lua, "SendMessageI_disablemusicreset", SendMessageI_disablemusicreset);
	lua_register(lua, "SendMessageI_fireweaponinstant", SendMessageI_fireweaponinstant);
	lua_register(lua, "SendMessageI_loopanimationfrom", SendMessageI_loopanimationfrom);
	lua_register(lua, "SendMessageI_movewithanimation", SendMessageI_movewithanimation);
	lua_register(lua, "SendMessageI_playanimationfrom", SendMessageI_playanimationfrom);
	lua_register(lua, "SendMessageI_setactivatedformp", SendMessageI_setactivatedformp);
	lua_register(lua, "SendMessageI_promptvideonoskip", SendMessageI_promptvideonoskip);
	lua_register(lua, "SendMessageI_playsoundifsilent", SendMessageI_playsoundifsilent);
	lua_register(lua, "SendMessageI_setimagealignment", SendMessageI_setimagealignment);
	lua_register(lua, "SendMessageI_transporttoifused", SendMessageI_transporttoifused);
	lua_register(lua, "SendMessageI_serverendplay", SendMessageI_serverendplay);
	lua_register(lua, "SendMessageI_activatemouse", SendMessageI_activatemouse);
	lua_register(lua, "SendMessageI_addplayerammo", SendMessageI_addplayerammo);
	lua_register(lua, "SendMessageI_loopanimation", SendMessageI_loopanimation);
	lua_register(lua, "SendMessageI_musicplaytime", SendMessageI_musicplaytime);
	lua_register(lua, "SendMessageI_musicplayfade", SendMessageI_musicplayfade);
	lua_register(lua, "SendMessageI_playanimation", SendMessageI_playanimation);
	lua_register(lua, "SendMessageI_nameplatesoff", SendMessageI_nameplatesoff);
	lua_register(lua, "SendMessageI_refreshentity", SendMessageI_refreshentity);
	lua_register(lua, "SendMessageI_setsoundspeed", SendMessageI_setsoundspeed);
	lua_register(lua, "SendMessageI_stopanimation", SendMessageI_stopanimation);
	lua_register(lua, "SendMessageI_deactivatemouse", SendMessageI_deactivatemouse);
	lua_register(lua, "SendMessageI_addplayerhealth", SendMessageI_addplayerhealth);
	lua_register(lua, "SendMessageI_addplayerweapon", SendMessageI_addplayerweapon);
	lua_register(lua, "SendMessageI_getentityinzone", SendMessageI_getentityinzone);
	lua_register(lua, "SendMessageI_musicsetdefault", SendMessageI_musicsetdefault);
	lua_register(lua, "SendMessageI_playvideonoskip", SendMessageI_playvideonoskip);
	lua_register(lua, "SendMessageI_setentityhealth", SendMessageI_setentityhealth);
	lua_register(lua, "SendMessageI_setlightvisible", SendMessageI_setlightvisible);
	lua_register(lua, "SendMessageI_addplayerjetpack", SendMessageI_addplayerjetpack);
	lua_register(lua, "SendMessageI_musicplayinstant", SendMessageI_musicplayinstant);
	lua_register(lua, "SendMessageI_musicplaytimecue", SendMessageI_musicplaytimecue);
	lua_register(lua, "SendMessageI_musicsetfadetime", SendMessageI_musicsetfadetime);
	lua_register(lua, "SendMessageI_musicsetinterval", SendMessageI_musicsetinterval);
	lua_register(lua, "SendMessageI_serverrespawnall", SendMessageI_serverrespawnall);
	lua_register(lua, "SendMessageI_setlockcharacter", SendMessageI_setlockcharacter);
	lua_register(lua, "SendMessageI_spawn", SendMessageI_spawn);
	lua_register(lua, "SendMessageI_changeplayerweaponid", SendMessageI_changeplayerweaponid);
	lua_register(lua, "SendMessageI_setcharactersoundset", SendMessageI_setcharactersoundset);
	lua_register(lua, "SendMessageI_setcharactertostrafe", SendMessageI_setcharactertostrafe);
	lua_register(lua, "SendMessageI_charactercontrolarmed", SendMessageI_charactercontrolarmed);
	lua_register(lua, "SendMessageI_charactercontrollimbo", SendMessageI_charactercontrollimbo);
	lua_register(lua, "SendMessageI_charactercontrolstand", SendMessageI_charactercontrolstand);
	lua_register(lua, "SendMessageI_setcharactertowalkrun", SendMessageI_setcharactertowalkrun);
	lua_register(lua, "SendMessageI_setentityhealthsilent", SendMessageI_setentityhealthsilent);
	lua_register(lua, "SendMessageI_charactercontrolducked", SendMessageI_charactercontrolducked);
	lua_register(lua, "SendMessageI_charactercontrolfidget", SendMessageI_charactercontrolfidget);
	lua_register(lua, "SendMessageI_charactercontrolmanual", SendMessageI_charactercontrolmanual);
	lua_register(lua, "SendMessageI_charactercontrolunarmed", SendMessageI_charactercontrolunarmed);
	lua_register(lua, "SendMessageI_performlogicconnections", SendMessageI_performlogicconnections);
	lua_register(lua, "SendMessageI_performlogicconnectionnumber", SendMessageI_performlogicconnectionnumber);
	lua_register(lua, "SendMessageI_setcharactervisiondelay", SendMessageI_setcharactervisiondelay);
	lua_register(lua, "SendMessageI_transporttofreezeposition", SendMessageI_transporttofreezeposition);
	lua_register(lua, "SendMessageI_setentityhealthwithdamage", SendMessageI_setentityhealthwithdamage);
	lua_register(lua, "SendMessageI_performlogicconnectionsaskey", SendMessageI_performlogicconnectionsaskey);
	lua_register(lua, "SendMessageI_setprioritytotransporter", SendMessageI_setprioritytotransporter);
}

// Internal float commands:
//lookatangle t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_lookatangle(); }
//lookforward t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_lookforward(); }
//moveforward t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_moveforward(); }
//rotatelimbx t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_rotatelimbx(); }
//rotatelimby t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_rotatelimby(); }
//rotatelimbz t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_rotatelimbz(); }
//setfoggreen t.v_f = LuaMessageFloat(); lua_setfoggreen(); }
//setsurfaceblue t.v_f = LuaMessageFloat(); lua_setsurfaceblue(); }
//setterrainsize t.v_f = LuaMessageFloat(); lua_setterrainsize(); }
//setambiencered t.v_f = LuaMessageFloat(); lua_setambiencered(); }
//resetpositionx t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_resetpositionx(); }
//resetpositiony t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_resetpositiony(); }
//resetpositionz t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_resetpositionz(); }
//resetrotationx t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_resetrotationx(); }
//resetrotationy t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_resetrotationy(); }
//resetrotationz t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_resetrotationz(); }
//setfogdistance t.v_f = LuaMessageFloat(); lua_setfogdistance(); }
//sethoverfactor t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_sethoverfactor(); }
//setcameraweaponfov t.v_f = LuaMessageFloat(); lua_setcameraweaponfov(); }
//setfreezepositionx t.v_f = LuaMessageFloat(); lua_setfreezepositionx(); }
//setfreezepositiony t.v_f = LuaMessageFloat(); lua_setfreezepositiony(); }
//setfreezepositionz t.v_f = LuaMessageFloat(); lua_setfreezepositionz(); }
//setoptionlightrays t.v_f = LuaMessageFloat(); lua_setoptionlightrays(); }
//setoptionocclusion t.v_f = LuaMessageFloat(); lua_setoptionocclusion(); }
//setvegetationwidth t.v_f = LuaMessageFloat(); lua_setvegetationwidth(); }
//setfreezepositionax t.v_f = LuaMessageFloat(); lua_setfreezepositionax(); }
//setfreezepositionay t.v_f = LuaMessageFloat(); lua_setfreezepositionay(); }
//setfreezepositionaz t.v_f = LuaMessageFloat(); lua_setfreezepositionaz(); }
//setoptionreflection t.v_f = LuaMessageFloat(); lua_setoptionreflection(); }
//setoptionvegetation t.v_f = LuaMessageFloat(); lua_setoptionvegetation(); }
//setpostsaointensity t.v_f = LuaMessageFloat(); lua_setpostsaointensity(); }
//setsurfaceintensity t.v_f = LuaMessageFloat(); lua_setsurfaceintensity(); }
//setsurfacesunfactor t.v_f = LuaMessageFloat(); lua_setsurfacesunfactor(); }
//setvegetationheight t.v_f = LuaMessageFloat(); lua_setvegetationheight(); }
//stopparticleemitter t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); lua_stopparticleemitter(); }
//lookattargetyoffset t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_lookattargetyoffset(); }
//setconstrast t.v_f = LuaMessageFloat(); lua_setconstrast(); }
//setcamerafov t.v_f = LuaMessageFloat(); lua_setcamerafov(); }
//lookatplayer t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_lookatplayer(); }
//lookattarget t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_lookattarget(); }
//movebackward t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_movebackward(); }
//mp_aimovetox t.e = LuaMessageIndex(); t.tSteamX_f = LuaMessageFloat(); }
//mp_aimovetoz t.e = LuaMessageIndex(); t.tSteamZ_f = LuaMessageFloat(); mp_COOP_aiMoveTo(); }
//setrotationx t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_setrotationx(); }
//setrotationy t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_setrotationy(); }
//setrotationz t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_setrotationz(); }
//setpositionx t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_setpositionx(); }
//setpositiony t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_setpositiony(); }
//setpositionz t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_setpositionz(); }
//setpostbloom t.v_f = LuaMessageFloat(); lua_setpostbloom(); }
//moveup t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_moveup(); }
//panelx t.luaPanel.x = LuaMessageFloat(); }
//panely t.luaPanel.y = LuaMessageFloat(); }
//panelx2 t.luaPanel.x2 = LuaMessageFloat(); }
//panely2 t.luaPanel.y2 = LuaMessageFloat(); }
//rotatex t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_rotatex(); }
//rotatey t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_rotatey(); }
//rotatez t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_rotatez(); }
//setfogred t.v_f = LuaMessageFloat(); lua_setfogred(); }
//setfogblue t.v_f = LuaMessageFloat(); lua_setfogblue(); }
//setcameradistance t.v_f = LuaMessageFloat(); lua_setcameradistance(); }
//setanimationframe t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_setanimationframe(); }
//setanimationspeed t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_setanimationspeed(); }
//setglobalspecular t.v_f = LuaMessageFloat(); lua_setglobalspecular(); }
//setimagepositionx t.v_f = LuaMessageFloat(); lua_setimagepositionx(); }
//setimagepositiony t.v_f = LuaMessageFloat(); lua_setimagepositiony(); }
//setterrainlodnear t.v_f = LuaMessageFloat(); lua_setterrainlodnear(); }
//setbrightness t.v_f = LuaMessageFloat(); lua_setbrightness(); }
//aimsmoothmode t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_aimsmoothmode(); }
//lookattargete t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_lookattargete(); }
//modulatespeed t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_modulatespeed(); }
//setfognearest t.v_f = LuaMessageFloat(); lua_setfognearest(); }
//setsurfacered t.v_f = LuaMessageFloat(); lua_setsurfacered(); }
//setambienceblue t.v_f = LuaMessageFloat(); lua_setambienceblue(); }
//setfogintensity t.v_f = LuaMessageFloat(); lua_setfogintensity(); }
//setsurfacegreen t.v_f = LuaMessageFloat(); lua_setsurfacegreen(); }
//setambiencegreen t.v_f = LuaMessageFloat(); lua_setambiencegreen(); }
//setoptionshadows t.v_f = LuaMessageFloat(); lua_setoptionshadows(); }
//setpostsaoradius t.v_f = LuaMessageFloat(); lua_setpostsaoradius(); }
//setterrainlodfar t.v_f = LuaMessageFloat(); lua_setterrainlodfar(); }
//setterrainlodmid t.v_f = LuaMessageFloat(); lua_setterrainlodmid(); }
//scale t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_scale(); }
//textx t.luaText.x = LuaMessageFloat(); t.tluaTextCenterX = 0; }
//texty t.luaText.y = LuaMessageFloat(); }
//promptlocalforvrmode t.v_f = LuaMessageFloat(); lua_promptlocalforvrmode(); }
//setambienceintensity t.v_f = LuaMessageFloat(); lua_setambienceintensity(); }
//setpostlightraydecay t.v_f = LuaMessageFloat(); lua_setpostlightraydecay(); }
//startparticleemitter t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); lua_startparticleemitter(); }
//setpostlightraylength t.v_f = LuaMessageFloat(); lua_setpostlightraylength(); }
//setpostmotiondistance t.v_f = LuaMessageFloat(); lua_setpostmotiondistance(); }
//setpostvignetteradius t.v_f = LuaMessageFloat(); lua_setpostvignetteradius(); }
//setvegetationquantity t.v_f = LuaMessageFloat(); lua_setvegetationquantity(); }
//setpostmotionintensity t.v_f = LuaMessageFloat(); lua_setpostmotionintensity(); }
//setpostlightrayquality t.v_f = LuaMessageFloat(); lua_setpostlightrayquality(); }
//setcamerazoompercentage t.v_f = LuaMessageFloat(); lua_setcamerazoompercentage(); }
//rotatetoplayerwithoffset t.e = LuaMessageIndex(); t.v_f = LuaMessageFloat(); entity_lua_rotatetoplayerwithoffset(); } fixed!
//setpostvignetteintensity t.v_f = LuaMessageFloat(); lua_setpostvignetteintensity(); }
//setpostlensflareintensity t.v_f = LuaMessageFloat(); lua_setpostlensflareintensity(); }
//setpostdepthoffielddistance t.v_f = LuaMessageFloat(); lua_setpostdepthoffielddistance(); }
//setpostdepthoffieldintensity t.v_f = LuaMessageFloat(); lua_setpostdepthoffieldintensity(); }
int int_core_sendmessagef(lua_State* L, eInternalCommandNames eInternalCommandValue)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int storee = t.e;
	float storev = t.v_f;
	if (n == 1)
	{
		t.v_f = lua_tonumber(L, 1);
	}
	if (n == 2)
	{
		t.e = lua_tonumber(L, 1);
		t.v_f = lua_tonumber(L, 2);
	}
	if (!LuaEntityCommandValid(L, eInternalCommandValue))
	{
		t.e = storee;
		t.v_f = storev;
		return 0;
	}
	switch (eInternalCommandValue)
	{
		case enum_lookatangle: entity_lua_lookatangle(); break;
		case enum_lookforward: entity_lua_lookforward(); break;
		case enum_moveforward: entity_lua_moveforward(); break;
		case enum_rotatelimbx: entity_lua_rotatelimbx(); break;
		case enum_rotatelimby: entity_lua_rotatelimby(); break;
		case enum_rotatelimbz: entity_lua_rotatelimbz(); break;
		case enum_setfoggreen: lua_setfoggreen(); break;
		case enum_setsurfaceblue: lua_setsurfaceblue(); break;
		case enum_setterrainsize: lua_setterrainsize(); break;
		case enum_setambiencered: lua_setambiencered(); break;
		case enum_resetpositionx: entity_lua_resetpositionx(); break;
		case enum_resetpositiony: entity_lua_resetpositiony(); break;
		case enum_resetpositionz: entity_lua_resetpositionz(); break;
		case enum_resetrotationx: entity_lua_resetrotationx(); break;
		case enum_resetrotationy: entity_lua_resetrotationy(); break;
		case enum_resetrotationz: entity_lua_resetrotationz(); break;
		case enum_setfogdistance: lua_setfogdistance(); break;
		case enum_sethoverfactor: entity_lua_sethoverfactor(); break;
		case enum_changeplayerweapon: entity_lua_changeplayerweapon(); break;
		case enum_playcharactersound: character_sound_play(); break;
		case enum_setcameraweaponfov: lua_setcameraweaponfov(); break;
		case enum_setanimationframes: entity_lua_setanimationframes(); break;
		case enum_setfreezepositionx: lua_setfreezepositionx(); break;
		case enum_setfreezepositiony: lua_setfreezepositiony(); break;
		case enum_setfreezepositionz: lua_setfreezepositionz(); break;
		case enum_setgamemusicvolume: lua_setgamemusicvolume(); break;
		case enum_setgamesoundvolume: lua_setgamesoundvolume(); break;
		case enum_setloadingresource: lua_setloadingresource(); break;
		case enum_setoptionlightrays: lua_setoptionlightrays(); break;
		case enum_setoptionocclusion: lua_setoptionocclusion(); break;
		case enum_setvegetationwidth: lua_setvegetationwidth(); break;
		case enum_levelfilenametoload: lua_levelfilenametoload(); break;
		case enum_replaceplayerweapon: entity_lua_replaceplayerweapon(); break;
		case enum_setfreezepositionax: lua_setfreezepositionax(); break;
		case enum_setfreezepositionay: lua_setfreezepositionay(); break;
		case enum_setfreezepositionaz: lua_setfreezepositionaz(); break;
		case enum_setoptionreflection: lua_setoptionreflection(); break;
		case enum_setoptionvegetation: lua_setoptionvegetation(); break;
		case enum_setpostsaointensity: lua_setpostsaointensity(); break;
		case enum_setsurfaceintensity: lua_setsurfaceintensity(); break;
		case enum_setsurfacesunfactor: lua_setsurfacesunfactor(); break;
		case enum_setvegetationheight: lua_setvegetationheight(); break;
		case enum_stopparticleemitter: lua_stopparticleemitter(); break;
		case enum_lookattargetyoffset: entity_lua_lookattargetyoffset(); break;
		case enum_setconstrast: lua_setconstrast(); break;
		case enum_setcamerafov: lua_setcamerafov(); break;
		case enum_lookattarget: entity_lua_lookattarget(); break;
		case enum_lookatplayer : entity_lua_lookatplayer(); break;
		case enum_movebackward: entity_lua_movebackward(); break;
		case enum_mp_aimovetox: t.tSteamX_f = LuaMessageFloat(); break;
		case enum_mp_aimovetoz: t.tSteamZ_f = LuaMessageFloat(); mp_COOP_aiMoveTo(); break;
		case enum_setrotationx : entity_lua_setrotationx(); break;
		case enum_setrotationy: entity_lua_setrotationy(); break;
		case enum_setrotationz: entity_lua_setrotationz(); break;
		case enum_setpositionx: entity_lua_setpositionx(); break;
		case enum_setpositiony: entity_lua_setpositiony(); break;
		case enum_setpositionz: entity_lua_setpositionz(); break;
		case enum_setpostbloom: lua_setpostbloom(); break;
		case enum_moveup: entity_lua_moveup(); break;
		case enum_panelx: t.luaPanel.x = LuaMessageFloat(); break;
		case enum_panely: t.luaPanel.y = LuaMessageFloat(); break;
		case enum_panelx2: t.luaPanel.x2 = LuaMessageFloat(); break;
		case enum_panely2: t.luaPanel.y2 = LuaMessageFloat(); break;
		case enum_rotatex: entity_lua_rotatex(); break;
		case enum_rotatey: entity_lua_rotatey(); break;
		case enum_rotatez: entity_lua_rotatez(); break;
		case enum_setfogred: lua_setfogred(); break;
		case enum_setfogblue: lua_setfogblue(); break;
		case enum_setcameradistance: lua_setcameradistance(); break;
		case enum_setanimationframe: entity_lua_setanimationframe(); break;
		case enum_changeanimationframe: entity_lua_changeanimationframe(); break;
		case enum_setanimationspeed: entity_lua_setanimationspeed(); break;
		case enum_setglobalspecular: lua_setglobalspecular(); break;
		case enum_setimagepositionx: lua_setimagepositionx(); break;
		case enum_setimagepositiony: lua_setimagepositiony(); break;
		case enum_setterrainlodnear: lua_setterrainlodnear(); break;
		case enum_setbrightness: lua_setbrightness(); break;
		case enum_aimsmoothmode: entity_lua_aimsmoothmode(); break;
		case enum_lookattargete: entity_lua_lookattargete(); break;
		case enum_modulatespeed: entity_lua_modulatespeed(); break;
		case enum_setfognearest: lua_setfognearest(); break;
		case enum_setsurfacered: lua_setsurfacered(); break;
		case enum_setambienceblue: lua_setambienceblue(); break;
		case enum_setfogintensity: lua_setfogintensity(); break;
		case enum_setsurfacegreen: lua_setsurfacegreen(); break;
		case enum_setambiencegreen: lua_setambiencegreen(); break;
		case enum_setoptionshadows: lua_setoptionshadows(); break;
		case enum_setpostsaoradius: lua_setpostsaoradius(); break;
		case enum_setterrainlodfar: lua_setterrainlodfar(); break;
		case enum_setterrainlodmid: lua_setterrainlodmid(); break;
		case enum_scale: entity_lua_scale(); break;
		case enum_textx: t.luaText.x = t.v_f; t.tluaTextCenterX = 0; break;
		case enum_texty: t.luaText.y = t.v_f; break;
		case enum_promptlocalforvrmode: lua_promptlocalforvrmode(); break;
		case enum_setambienceintensity: lua_setambienceintensity(); break;
		case enum_setpostlightraydecay: lua_setpostlightraydecay(); break;
		case enum_startparticleemitter: lua_startparticleemitter(); break;
		case enum_setpostlightraylength: lua_setpostlightraylength(); break;
		case enum_setpostmotiondistance: lua_setpostmotiondistance(); break;
		case enum_setpostvignetteradius: lua_setpostvignetteradius(); break;
		case enum_setvegetationquantity: lua_setvegetationquantity(); break;
		case enum_setpostmotionintensity: lua_setpostmotionintensity(); break;
		case enum_setpostlightrayquality: lua_setpostlightrayquality(); break;
		case enum_setcamerazoompercentage: lua_setcamerazoompercentage(); break;
		case enum_rotatetoplayerwithoffset: entity_lua_rotatetoplayerwithoffset(); break;
		case enum_setpostvignetteintensity: lua_setpostvignetteintensity(); break;
		case enum_setpostlensflareintensity: lua_setpostlensflareintensity(); break;
		case enum_setpostdepthoffielddistance: lua_setpostdepthoffielddistance(); break;
		case enum_setpostdepthoffieldintensity: lua_setpostdepthoffieldintensity(); break;
	}
	t.e = storee;
	t.v_f = storev;
	return 0;
}
int SendMessageF_lookatangle(lua_State* L) { return int_core_sendmessagef(L, enum_lookatangle); }
int SendMessageF_lookforward(lua_State* L) { return int_core_sendmessagef(L, enum_lookforward); }
int SendMessageF_moveforward(lua_State* L) { return int_core_sendmessagef(L, enum_moveforward); }
int SendMessageF_rotatelimbx(lua_State* L) { return int_core_sendmessagef(L, enum_rotatelimbx); }
int SendMessageF_rotatelimby(lua_State* L) { return int_core_sendmessagef(L, enum_rotatelimby); }
int SendMessageF_rotatelimbz(lua_State* L) { return int_core_sendmessagef(L, enum_rotatelimbz); }
int SendMessageF_setfoggreen(lua_State* L) { return int_core_sendmessagef(L, enum_setfoggreen); }
int SendMessageF_texty(lua_State* L) { return int_core_sendmessagef(L, enum_texty); }
int SendMessageF_setsurfaceblue(lua_State* L) { return int_core_sendmessagef(L, enum_setsurfaceblue); }
int SendMessageF_setterrainsize(lua_State* L) { return int_core_sendmessagef(L, enum_setterrainsize); }
int SendMessageF_setambiencered(lua_State* L) { return int_core_sendmessagef(L, enum_setambiencered); }
int SendMessageF_resetpositionx(lua_State* L) { return int_core_sendmessagef(L, enum_resetpositionx); }
int SendMessageF_resetpositiony(lua_State* L) { return int_core_sendmessagef(L, enum_resetpositiony); }
int SendMessageF_resetpositionz(lua_State* L) { return int_core_sendmessagef(L, enum_resetpositionz); }
int SendMessageF_resetrotationx(lua_State* L) { return int_core_sendmessagef(L, enum_resetrotationx); }
int SendMessageF_resetrotationy(lua_State* L) { return int_core_sendmessagef(L, enum_resetrotationy); }
int SendMessageF_resetrotationz(lua_State* L) { return int_core_sendmessagef(L, enum_resetrotationz); }
int SendMessageF_setfogdistance(lua_State* L) { return int_core_sendmessagef(L, enum_setfogdistance); }
int SendMessageF_sethoverfactor(lua_State* L) { return int_core_sendmessagef(L, enum_sethoverfactor); }
int SendMessageF_changeplayerweapon(lua_State* L) { return int_core_sendmessagef(L, enum_changeplayerweapon); }
int SendMessageF_playcharactersound(lua_State* L) { return int_core_sendmessagef(L, enum_playcharactersound); }
int SendMessageF_setcameraweaponfov(lua_State* L) { return int_core_sendmessagef(L, enum_setcameraweaponfov); }
int SendMessageF_setanimationframes(lua_State* L) { return int_core_sendmessagef(L, enum_setanimationframes); }
int SendMessageF_setfreezepositionx(lua_State* L) { return int_core_sendmessagef(L, enum_setfreezepositionx); }
int SendMessageF_setfreezepositiony(lua_State* L) { return int_core_sendmessagef(L, enum_setfreezepositiony); }
int SendMessageF_setfreezepositionz(lua_State* L) { return int_core_sendmessagef(L, enum_setfreezepositionz); }
int SendMessageF_setgamemusicvolume(lua_State* L) { return int_core_sendmessagef(L, enum_setgamemusicvolume); }
int SendMessageF_setgamesoundvolume(lua_State* L) { return int_core_sendmessagef(L, enum_setgamesoundvolume); }
int SendMessageF_setloadingresource(lua_State* L) { return int_core_sendmessagef(L, enum_setloadingresource); }
int SendMessageF_setoptionlightrays(lua_State* L) { return int_core_sendmessagef(L, enum_setoptionlightrays); }
int SendMessageF_setoptionocclusion(lua_State* L) { return int_core_sendmessagef(L, enum_setoptionocclusion); }
int SendMessageF_setvegetationwidth(lua_State* L) { return int_core_sendmessagef(L, enum_setvegetationwidth); }
int SendMessageF_levelfilenametoload(lua_State* L) { return int_core_sendmessagef(L, enum_levelfilenametoload); }
int SendMessageF_replaceplayerweapon(lua_State* L) { return int_core_sendmessagef(L, enum_replaceplayerweapon); }
int SendMessageF_setfreezepositionax(lua_State* L) { return int_core_sendmessagef(L, enum_setfreezepositionax); }
int SendMessageF_setfreezepositionay(lua_State* L) { return int_core_sendmessagef(L, enum_setfreezepositionay); }
int SendMessageF_setfreezepositionaz(lua_State* L) { return int_core_sendmessagef(L, enum_setfreezepositionaz); }
int SendMessageF_setoptionreflection(lua_State* L) { return int_core_sendmessagef(L, enum_setoptionreflection); }
int SendMessageF_setoptionvegetation(lua_State* L) { return int_core_sendmessagef(L, enum_setoptionvegetation); }
int SendMessageF_setpostsaointensity(lua_State* L) { return int_core_sendmessagef(L, enum_setpostsaointensity); }
int SendMessageF_setsurfaceintensity(lua_State* L) { return int_core_sendmessagef(L, enum_setsurfaceintensity); }
int SendMessageF_setsurfacesunfactor(lua_State* L) { return int_core_sendmessagef(L, enum_setsurfacesunfactor); }
int SendMessageF_setvegetationheight(lua_State* L) { return int_core_sendmessagef(L, enum_setvegetationheight); }
int SendMessageF_stopparticleemitter(lua_State* L) { return int_core_sendmessagef(L, enum_stopparticleemitter); }
int SendMessageF_lookattargetyoffset(lua_State* L) { return int_core_sendmessagef(L, enum_lookattargetyoffset); }
int SendMessageF_setconstrast(lua_State* L) { return int_core_sendmessagef(L, enum_setconstrast); }
int SendMessageF_setcamerafov(lua_State* L) { return int_core_sendmessagef(L, enum_setcamerafov); }
int SendMessageF_lookattarget(lua_State* L) { return int_core_sendmessagef(L, enum_lookattarget); }
int SendMessageF_lookatplayer(lua_State* L) { return int_core_sendmessagef(L, enum_lookatplayer); }
int SendMessageF_movebackward(lua_State* L) { return int_core_sendmessagef(L, enum_movebackward); }
int SendMessageF_mp_aimovetox(lua_State* L) { return int_core_sendmessagef(L, enum_mp_aimovetox); }
int SendMessageF_mp_aimovetoz(lua_State* L) { return int_core_sendmessagef(L, enum_mp_aimovetoz); }
int SendMessageF_setrotationx(lua_State* L) { return int_core_sendmessagef(L, enum_setrotationx); }
int SendMessageF_setrotationy(lua_State* L) { return int_core_sendmessagef(L, enum_setrotationy); }
int SendMessageF_setrotationz(lua_State* L) { return int_core_sendmessagef(L, enum_setrotationz); }
int SendMessageF_setpositionx(lua_State* L) { return int_core_sendmessagef(L, enum_setpositionx); }
int SendMessageF_setpositiony(lua_State* L) { return int_core_sendmessagef(L, enum_setpositiony); }
int SendMessageF_setpositionz(lua_State* L) { return int_core_sendmessagef(L, enum_setpositionz); }
int SendMessageF_setpostbloom(lua_State* L) { return int_core_sendmessagef(L, enum_setpostbloom); }
int SendMessageF_moveup(lua_State* L) { return int_core_sendmessagef(L, enum_moveup); }
int SendMessageF_panelx(lua_State* L) { return int_core_sendmessagef(L, enum_panelx); }
int SendMessageF_panely(lua_State* L) { return int_core_sendmessagef(L, enum_panely); }
int SendMessageF_panelx2(lua_State* L) { return int_core_sendmessagef(L, enum_panelx2); }
int SendMessageF_panely2(lua_State* L) { return int_core_sendmessagef(L, enum_panely2); }
int SendMessageF_rotatex(lua_State* L) { return int_core_sendmessagef(L, enum_rotatex); }
int SendMessageF_rotatey(lua_State* L) { return int_core_sendmessagef(L, enum_rotatey); }
int SendMessageF_rotatez(lua_State* L) { return int_core_sendmessagef(L, enum_rotatez); }
int SendMessageF_setfogred(lua_State* L) { return int_core_sendmessagef(L, enum_setfogred); }
int SendMessageF_setfogblue(lua_State* L) { return int_core_sendmessagef(L, enum_setfogblue); }
int SendMessageF_setcameradistance(lua_State* L) { return int_core_sendmessagef(L, enum_setcameradistance); }
int SendMessageF_setanimationframe(lua_State* L) { return int_core_sendmessagef(L, enum_setanimationframe); }
int SendMessageF_changeanimationframe(lua_State* L) { return int_core_sendmessagef(L, enum_changeanimationframe); }
int SendMessageF_setanimationspeed(lua_State* L) { return int_core_sendmessagef(L, enum_setanimationspeed); }
int SendMessageF_setglobalspecular(lua_State* L) { return int_core_sendmessagef(L, enum_setglobalspecular); }
int SendMessageF_setimagepositionx(lua_State* L) { return int_core_sendmessagef(L, enum_setimagepositionx); }
int SendMessageF_setimagepositiony(lua_State* L) { return int_core_sendmessagef(L, enum_setimagepositiony); }
int SendMessageF_setterrainlodnear(lua_State* L) { return int_core_sendmessagef(L, enum_setterrainlodnear); }
int SendMessageF_setbrightness(lua_State* L) { return int_core_sendmessagef(L, enum_setbrightness); }
int SendMessageF_aimsmoothmode(lua_State* L) { return int_core_sendmessagef(L, enum_aimsmoothmode); }
int SendMessageF_lookattargete(lua_State* L) { return int_core_sendmessagef(L, enum_lookattargete); }
int SendMessageF_modulatespeed(lua_State* L) { return int_core_sendmessagef(L, enum_modulatespeed); }
int SendMessageF_setfognearest(lua_State* L) { return int_core_sendmessagef(L, enum_setfognearest); }
int SendMessageF_setsurfacered(lua_State* L) { return int_core_sendmessagef(L, enum_setsurfacered); }
int SendMessageF_setambienceblue(lua_State* L) { return int_core_sendmessagef(L, enum_setambienceblue); }
int SendMessageF_setfogintensity(lua_State* L) { return int_core_sendmessagef(L, enum_setfogintensity); }
int SendMessageF_setsurfacegreen(lua_State* L) { return int_core_sendmessagef(L, enum_setsurfacegreen); }
int SendMessageF_setambiencegreen(lua_State* L) { return int_core_sendmessagef(L, enum_setambiencegreen); }
int SendMessageF_setoptionshadows(lua_State* L) { return int_core_sendmessagef(L, enum_setoptionshadows); }
int SendMessageF_setpostsaoradius(lua_State* L) { return int_core_sendmessagef(L, enum_setpostsaoradius); }
int SendMessageF_setterrainlodfar(lua_State* L) { return int_core_sendmessagef(L, enum_setterrainlodfar); }
int SendMessageF_setterrainlodmid(lua_State* L) { return int_core_sendmessagef(L, enum_setterrainlodmid); }
int SendMessageF_scale(lua_State* L) { return int_core_sendmessagef(L, enum_scale); }
int SendMessageF_textx(lua_State* L) { return int_core_sendmessagef(L, enum_textx); }
int SendMessageF_promptlocalforvrmode(lua_State* L) { return int_core_sendmessagef(L, enum_promptlocalforvrmode); }
int SendMessageF_setambienceintensity(lua_State* L) { return int_core_sendmessagef(L, enum_setambienceintensity); }
int SendMessageF_setpostlightraydecay(lua_State* L) { return int_core_sendmessagef(L, enum_setpostlightraydecay); }
int SendMessageF_startparticleemitter(lua_State* L) { return int_core_sendmessagef(L, enum_startparticleemitter); }
int SendMessageF_setpostlightraylength(lua_State* L) { return int_core_sendmessagef(L, enum_setpostlightraylength); }
int SendMessageF_setpostmotiondistance(lua_State* L) { return int_core_sendmessagef(L, enum_setpostmotiondistance); }
int SendMessageF_setpostvignetteradius(lua_State* L) { return int_core_sendmessagef(L, enum_setpostvignetteradius); }
int SendMessageF_setvegetationquantity(lua_State* L) { return int_core_sendmessagef(L, enum_setvegetationquantity); }
int SendMessageF_setpostmotionintensity(lua_State* L) { return int_core_sendmessagef(L, enum_setpostmotionintensity); }
int SendMessageF_setpostlightrayquality(lua_State* L) { return int_core_sendmessagef(L, enum_setpostlightrayquality); }
int SendMessageF_setcamerazoompercentage(lua_State* L) { return int_core_sendmessagef(L, enum_setcamerazoompercentage); }
int SendMessageF_rotatetoplayerwithoffset(lua_State* L) { return int_core_sendmessagef(L, enum_rotatetoplayerwithoffset); }
int SendMessageF_setpostvignetteintensity(lua_State* L) { return int_core_sendmessagef(L, enum_setpostvignetteintensity); }
int SendMessageF_setpostlensflareintensity(lua_State* L) { return int_core_sendmessagef(L, enum_setpostlensflareintensity); }
int SendMessageF_setpostdepthoffielddistance(lua_State* L) { return int_core_sendmessagef(L, enum_setpostdepthoffielddistance); }
int SendMessageF_setpostdepthoffieldintensity(lua_State* L) { return int_core_sendmessagef(L, enum_setpostdepthoffieldintensity); }
void addInternalFunctions_float()
{
	lua_register(lua, "SendMessageF_lookatangle", SendMessageF_lookatangle);
	lua_register(lua, "SendMessageF_lookforward", SendMessageF_lookforward);
	lua_register(lua, "SendMessageF_moveforward", SendMessageF_moveforward);
	lua_register(lua, "SendMessageF_rotatelimbx", SendMessageF_rotatelimbx);
	lua_register(lua, "SendMessageF_rotatelimby", SendMessageF_rotatelimby);
	lua_register(lua, "SendMessageF_rotatelimbz", SendMessageF_rotatelimbz);
	lua_register(lua, "SendMessageF_setfoggreen", SendMessageF_setfoggreen);
	lua_register(lua, "SendMessageF_texty", SendMessageF_texty);
	lua_register(lua, "SendMessageF_setsurfaceblue", SendMessageF_setsurfaceblue);
	lua_register(lua, "SendMessageF_setterrainsize", SendMessageF_setterrainsize);
	lua_register(lua, "SendMessageF_setambiencered", SendMessageF_setambiencered);
	lua_register(lua, "SendMessageF_resetpositionx", SendMessageF_resetpositionx);
	lua_register(lua, "SendMessageF_resetpositiony", SendMessageF_resetpositiony);
	lua_register(lua, "SendMessageF_resetpositionz", SendMessageF_resetpositionz);
	lua_register(lua, "SendMessageF_resetrotationx", SendMessageF_resetrotationx);
	lua_register(lua, "SendMessageF_resetrotationy", SendMessageF_resetrotationy);
	lua_register(lua, "SendMessageF_resetrotationz", SendMessageF_resetrotationz);
	lua_register(lua, "SendMessageF_setfogdistance", SendMessageF_setfogdistance);
	lua_register(lua, "SendMessageF_sethoverfactor", SendMessageF_sethoverfactor);
	lua_register(lua, "SendMessageF_changeplayerweapon", SendMessageF_changeplayerweapon);
	lua_register(lua, "SendMessageF_playcharactersound", SendMessageF_playcharactersound);
	lua_register(lua, "SendMessageF_setcameraweaponfov", SendMessageF_setcameraweaponfov);
	lua_register(lua, "SendMessageF_setanimationframes", SendMessageF_setanimationframes);
	lua_register(lua, "SendMessageF_setfreezepositionx", SendMessageF_setfreezepositionx);
	lua_register(lua, "SendMessageF_setfreezepositiony", SendMessageF_setfreezepositiony);
	lua_register(lua, "SendMessageF_setfreezepositionz", SendMessageF_setfreezepositionz);
	lua_register(lua, "SendMessageF_setgamemusicvolume", SendMessageF_setgamemusicvolume);
	lua_register(lua, "SendMessageF_setgamesoundvolume", SendMessageF_setgamesoundvolume);
	lua_register(lua, "SendMessageF_setloadingresource", SendMessageF_setloadingresource);
	lua_register(lua, "SendMessageF_setoptionlightrays", SendMessageF_setoptionlightrays);
	lua_register(lua, "SendMessageF_setoptionocclusion", SendMessageF_setoptionocclusion);
	lua_register(lua, "SendMessageF_setvegetationwidth", SendMessageF_setvegetationwidth);
	lua_register(lua, "SendMessageF_levelfilenametoload", SendMessageF_levelfilenametoload);
	lua_register(lua, "SendMessageF_replaceplayerweapon", SendMessageF_replaceplayerweapon);
	lua_register(lua, "SendMessageF_setfreezepositionax", SendMessageF_setfreezepositionax);
	lua_register(lua, "SendMessageF_setfreezepositionay", SendMessageF_setfreezepositionay);
	lua_register(lua, "SendMessageF_setfreezepositionaz", SendMessageF_setfreezepositionaz);
	lua_register(lua, "SendMessageF_setoptionreflection", SendMessageF_setoptionreflection);
	lua_register(lua, "SendMessageF_setoptionvegetation", SendMessageF_setoptionvegetation);
	lua_register(lua, "SendMessageF_setpostsaointensity", SendMessageF_setpostsaointensity);
	lua_register(lua, "SendMessageF_setsurfaceintensity", SendMessageF_setsurfaceintensity);
	lua_register(lua, "SendMessageF_setsurfacesunfactor", SendMessageF_setsurfacesunfactor);
	lua_register(lua, "SendMessageF_setvegetationheight", SendMessageF_setvegetationheight);
	lua_register(lua, "SendMessageF_stopparticleemitter", SendMessageF_stopparticleemitter);
	lua_register(lua, "SendMessageF_lookattargetyoffset", SendMessageF_lookattargetyoffset);
	lua_register(lua, "SendMessageF_setconstrast", SendMessageF_setconstrast);
	// global.lua SetContrast calls the correctly spelled name, keep the old one for any script using it directly
	lua_register(lua, "SendMessageF_setcontrast", SendMessageF_setconstrast);
	lua_register(lua, "SendMessageF_setcamerafov", SendMessageF_setcamerafov);
	lua_register(lua, "SendMessageF_lookattarget", SendMessageF_lookattarget);
	lua_register(lua, "SendMessageF_lookatplayer", SendMessageF_lookatplayer);
	lua_register(lua, "SendMessageF_movebackward", SendMessageF_movebackward);
	lua_register(lua, "SendMessageF_mp_aimovetox", SendMessageF_mp_aimovetox);
	lua_register(lua, "SendMessageF_mp_aimovetoz", SendMessageF_mp_aimovetoz);
	lua_register(lua, "SendMessageF_setrotationx", SendMessageF_setrotationx);
	lua_register(lua, "SendMessageF_setrotationy", SendMessageF_setrotationy);
	lua_register(lua, "SendMessageF_setrotationz", SendMessageF_setrotationz);
	lua_register(lua, "SendMessageF_setpositionx", SendMessageF_setpositionx);
	lua_register(lua, "SendMessageF_setpositiony", SendMessageF_setpositiony);
	lua_register(lua, "SendMessageF_setpositionz", SendMessageF_setpositionz);
	lua_register(lua, "SendMessageF_setpostbloom", SendMessageF_setpostbloom);
	lua_register(lua, "SendMessageF_moveup", SendMessageF_moveup);
	lua_register(lua, "SendMessageF_panelx", SendMessageF_panelx);
	lua_register(lua, "SendMessageF_panely", SendMessageF_panely);
	lua_register(lua, "SendMessageF_panelx2", SendMessageF_panelx2);
	lua_register(lua, "SendMessageF_panely2", SendMessageF_panely2);
	lua_register(lua, "SendMessageF_rotatex", SendMessageF_rotatex);
	lua_register(lua, "SendMessageF_rotatey", SendMessageF_rotatey);
	lua_register(lua, "SendMessageF_rotatez", SendMessageF_rotatez);
	lua_register(lua, "SendMessageF_setfogred", SendMessageF_setfogred);
	lua_register(lua, "SendMessageF_setfogblue", SendMessageF_setfogblue);
	lua_register(lua, "SendMessageF_setcameradistance", SendMessageF_setcameradistance);
	lua_register(lua, "SendMessageF_setanimationframe", SendMessageF_setanimationframe);
	lua_register(lua, "SendMessageF_changeanimationframe", SendMessageF_changeanimationframe);
	lua_register(lua, "SendMessageF_setanimationspeed", SendMessageF_setanimationspeed);
	lua_register(lua, "SendMessageF_setglobalspecular", SendMessageF_setglobalspecular);
	lua_register(lua, "SendMessageF_setimagepositionx", SendMessageF_setimagepositionx);
	lua_register(lua, "SendMessageF_setimagepositiony", SendMessageF_setimagepositiony);
	lua_register(lua, "SendMessageF_setterrainlodnear", SendMessageF_setterrainlodnear);
	lua_register(lua, "SendMessageF_setbrightness", SendMessageF_setbrightness);
	lua_register(lua, "SendMessageF_aimsmoothmode", SendMessageF_aimsmoothmode);
	lua_register(lua, "SendMessageF_lookattargete", SendMessageF_lookattargete);
	lua_register(lua, "SendMessageF_modulatespeed", SendMessageF_modulatespeed);
	lua_register(lua, "SendMessageF_setfognearest", SendMessageF_setfognearest);
	lua_register(lua, "SendMessageF_setsurfacered", SendMessageF_setsurfacered);
	lua_register(lua, "SendMessageF_setambienceblue", SendMessageF_setambienceblue);
	lua_register(lua, "SendMessageF_setfogintensity", SendMessageF_setfogintensity);
	lua_register(lua, "SendMessageF_setsurfacegreen", SendMessageF_setsurfacegreen);
	lua_register(lua, "SendMessageF_setambiencegreen", SendMessageF_setambiencegreen);
	lua_register(lua, "SendMessageF_setoptionshadows", SendMessageF_setoptionshadows);
	lua_register(lua, "SendMessageF_setpostsaoradius", SendMessageF_setpostsaoradius);
	lua_register(lua, "SendMessageF_setterrainlodfar", SendMessageF_setterrainlodfar);
	lua_register(lua, "SendMessageF_setterrainlodmid", SendMessageF_setterrainlodmid);
	lua_register(lua, "SendMessageF_scale", SendMessageF_scale);
	lua_register(lua, "SendMessageF_textx", SendMessageF_textx);
	lua_register(lua, "SendMessageF_promptlocalforvrmode", SendMessageF_promptlocalforvrmode);
	lua_register(lua, "SendMessageF_setambienceintensity", SendMessageF_setambienceintensity);
	lua_register(lua, "SendMessageF_setpostlightraydecay", SendMessageF_setpostlightraydecay);
	lua_register(lua, "SendMessageF_startparticleemitter", SendMessageF_startparticleemitter);
	lua_register(lua, "SendMessageF_setpostlightraylength", SendMessageF_setpostlightraylength);
	lua_register(lua, "SendMessageF_setpostmotiondistance", SendMessageF_setpostmotiondistance);
	lua_register(lua, "SendMessageF_setpostvignetteradius", SendMessageF_setpostvignetteradius);
	lua_register(lua, "SendMessageF_setvegetationquantity", SendMessageF_setvegetationquantity);
	lua_register(lua, "SendMessageF_setpostmotionintensity", SendMessageF_setpostmotionintensity);
	lua_register(lua, "SendMessageF_setpostlightrayquality", SendMessageF_setpostlightrayquality);
	lua_register(lua, "SendMessageF_setcamerazoompercentage", SendMessageF_setcamerazoompercentage);
	lua_register(lua, "SendMessageF_rotatetoplayerwithoffset", SendMessageF_rotatetoplayerwithoffset);
	lua_register(lua, "SendMessageF_setpostvignetteintensity", SendMessageF_setpostvignetteintensity);
	lua_register(lua, "SendMessageF_setpostlensflareintensity", SendMessageF_setpostlensflareintensity);
	lua_register(lua, "SendMessageF_setpostdepthoffielddistance", SendMessageF_setpostdepthoffielddistance);
	lua_register(lua, "SendMessageF_setpostdepthoffieldintensity", SendMessageF_setpostdepthoffieldintensity);
}

//Internal String commands:
//jumptolevel t.e = LuaMessageIndex(); t.s_s = LuaMessageString(); lua_jumptolevel(); }
//promptlocal t.e = LuaMessageIndex(); t.s_s = LuaMessageString(); lua_promptlocal(); }
//playcharactersound t.e = LuaMessageIndex(); t.s_s = LuaMessageString(); character_sound_play(); }
//promptduration t.v = LuaMessageIndex(); t.s_s = LuaMessageString(); lua_promptduration(); }
//changeplayerweapon t.s_s = LuaMessageString(); entity_lua_changeplayerweapon(); }
//levelfilenametoload t.s_s = LuaMessageString(); lua_levelfilenametoload(); }
//switchscript t.e = LuaMessageIndex(); t.s_s = LuaMessageString(); entity_lua_switchscript(); }
//prompt t.s_s = LuaMessageString(); lua_prompt(); }
//texttxt t.luaText.txt = LuaMessageString(); lua_text(); }
//setskyto t.s_s = LuaMessageString(); lua_set_sky(); }
//musicload t.m = LuaMessageIndex(); t.s_s = LuaMessageString(); lua_musicload(); }
//switchpage t.s_s = LuaMessageString(); lua_switchpage(); }
//setanimationname t.e = LuaMessageIndex(); t.s_s = LuaMessageString();  entity_lua_setanimationname(); }
//promptlocalforvr t.e = LuaMessageIndex(); t.s_s = LuaMessageString(); lua_promptlocalforvr(); }
//loadimages t.v = LuaMessageIndex(); t.s_s = LuaMessageString(); lua_loadimages(); }
//setcharactersound t.e = LuaMessageIndex(); t.s_s = LuaMessageString(); character_sound_load(); }
int int_core_sendmessages(lua_State* L, eInternalCommandNames eInternalCommandValue)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int storee = t.e;
	cstr stores = t.s_s;
	if (n == 1)
	{
		const char* pStrPtr = lua_tostring(L, 1);
		if(pStrPtr)
			t.s_s = pStrPtr;
		else
			t.s_s = "";
	}
	if (n == 2)
	{
		t.e = lua_tonumber(L, 1);
		const char* pStrPtr = lua_tostring(L, 2);
		if (pStrPtr)
			t.s_s = pStrPtr;
		else
			t.s_s = "";
	}
	if (!LuaEntityCommandValid(L, eInternalCommandValue))
	{
		t.e = storee;
		t.s_s = stores;
		return 0;
	}
	switch (eInternalCommandValue)
	{
		case enum_jumptolevel: lua_jumptolevel(); break;
		case enum_promptlocal: lua_promptlocal(); break;
		case enum_playcharactersound: character_sound_play(); break;
		case enum_promptduration: t.v = t.e; lua_promptduration(); break;
		case enum_changeplayerweapon: entity_lua_changeplayerweapon(); break;
		case enum_levelfilenametoload: lua_levelfilenametoload(); break;
		case enum_switchscript: entity_lua_switchscript(); break;
		case enum_prompt: lua_prompt(); break;
		case enum_texttxt: t.luaText.txt = t.s_s; lua_text(); break;
		case enum_setskyto: lua_set_sky(); break;
		case enum_musicload: t.m = t.e; lua_musicload(); break;
		case enum_switchpage: lua_switchpage(); break;
		case enum_setanimationname: entity_lua_setanimationname(); break;
		case enum_promptlocalforvr: lua_promptlocalforvr(); break;
		case enum_loadimages: t.v = t.e; lua_loadimages(); break;
		case enum_setcharactersound: character_sound_load(); break;
	}
	t.e = storee;
	t.s_s = stores;
	return 0;
}
int SendMessageS_jumptolevel(lua_State* L) { return int_core_sendmessages(L, enum_jumptolevel); }
int SendMessageS_promptlocal(lua_State* L) { return int_core_sendmessages(L, enum_promptlocal); }
int SendMessageS_playcharactersound(lua_State* L) { return int_core_sendmessages(L, enum_playcharactersound); }
int SendMessageS_promptduration(lua_State* L) { return int_core_sendmessages(L, enum_promptduration); }
int SendMessageS_changeplayerweapon(lua_State* L) { return int_core_sendmessages(L, enum_changeplayerweapon); }
int SendMessageS_levelfilenametoload(lua_State* L) { return int_core_sendmessages(L, enum_levelfilenametoload); }
int SendMessageS_switchscript(lua_State* L) { return int_core_sendmessages(L, enum_switchscript); }
int SendMessageS_prompt(lua_State* L) { return int_core_sendmessages(L, enum_prompt); }
int SendMessageS_texttxt(lua_State* L) { return int_core_sendmessages(L, enum_texttxt); }
int SendMessageS_setskyto(lua_State* L) { return int_core_sendmessages(L, enum_setskyto); }
int SendMessageS_musicload(lua_State* L) { return int_core_sendmessages(L, enum_musicload); }
int SendMessageS_switchpage(lua_State* L) { return int_core_sendmessages(L, enum_switchpage); }
int SendMessageS_setanimationname(lua_State* L) { return int_core_sendmessages(L, enum_setanimationname); }
int SendMessageS_promptlocalforvr(lua_State* L) { return int_core_sendmessages(L, enum_promptlocalforvr); }
int SendMessageS_loadimages(lua_State* L) { return int_core_sendmessages(L, enum_loadimages); }
int SendMessageS_setcharactersound(lua_State* L) { return int_core_sendmessages(L, enum_setcharactersound); }
void addInternalFunctions_string()
{
	lua_register(lua, "SendMessageS_jumptolevel", SendMessageS_jumptolevel);
	lua_register(lua, "SendMessageS_promptlocal", SendMessageS_promptlocal);
	lua_register(lua, "SendMessageS_playcharactersound", SendMessageS_playcharactersound);
	lua_register(lua, "SendMessageS_promptduration", SendMessageS_promptduration);
	lua_register(lua, "SendMessageS_changeplayerweapon", SendMessageS_changeplayerweapon);
	lua_register(lua, "SendMessageS_levelfilenametoload", SendMessageS_levelfilenametoload);
	lua_register(lua, "SendMessageS_switchscript", SendMessageS_switchscript);
	lua_register(lua, "SendMessageS_prompt", SendMessageS_prompt);
	lua_register(lua, "SendMessageS_texttxt", SendMessageS_texttxt);
	lua_register(lua, "SendMessageS_setskyto", SendMessageS_setskyto);
	lua_register(lua, "SendMessageS_musicload", SendMessageS_musicload);
	lua_register(lua, "SendMessageS_switchpage", SendMessageS_switchpage);
	lua_register(lua, "SendMessageS_setanimationname", SendMessageS_setanimationname);
	lua_register(lua, "SendMessageS_promptlocalforvr", SendMessageS_promptlocalforvr);
	lua_register(lua, "SendMessageS_loadimages", SendMessageS_loadimages);
	lua_register(lua, "SendMessageS_setcharactersound", SendMessageS_setcharactersound);
}

// LUA command names
void addFunctions()
{
	// add internal commands
	addInternalFunctions_nones();
	addInternalFunctions_integer();
	addInternalFunctions_float();
	addInternalFunctions_string();

	// add extra commands (old method)
	lua_register(lua, "SendMessage", LuaSendMessage);
	lua_register(lua, "SendMessageI", LuaSendMessageI);
	lua_register(lua, "SendMessageF", LuaSendMessageF);
	lua_register(lua, "SendMessageS", LuaSendMessageS);
	lua_register(lua, "Include", Include);

	// Direct Calls
	lua_register(lua, "RestoreGameFromSlot", RestoreGameFromSlot);
	lua_register(lua, "ResetFade", ResetFade);

	lua_register(lua, "GetInternalSoundState", GetInternalSoundState);
	lua_register(lua, "SetInternalSoundState", SetInternalSoundState);
	lua_register(lua, "SetCheckpoint", SetCheckpoint);

	lua_register(lua, "UpdateWeaponStats", UpdateWeaponStats);
	lua_register(lua, "ResetWeaponSystems", ResetWeaponSystems);
	lua_register(lua, "GetWeaponSlotGot", GetWeaponSlotGot);
	lua_register(lua, "GetWeaponSlotNoSelect", GetWeaponSlotNoSelect);
	lua_register(lua, "SetWeaponSlot", SetWeaponSlot);

	lua_register(lua, "GetWeaponAmmo", GetWeaponAmmo);
	lua_register(lua, "SetWeaponAmmo", SetWeaponAmmo);
	lua_register(lua, "GetWeaponClipAmmo", GetWeaponClipAmmo);
	lua_register(lua, "SetWeaponClipAmmo", SetWeaponClipAmmo);
	lua_register(lua, "GetWeaponPoolAmmoIndex", GetWeaponPoolAmmoIndex);
	lua_register(lua, "GetWeaponPoolAmmo", GetWeaponPoolAmmo);
	lua_register(lua, "SetWeaponPoolAmmo", SetWeaponPoolAmmo);

	lua_register(lua, "GetWeaponSlot", GetWeaponSlot);
	lua_register(lua, "GetWeaponSlotPref", GetWeaponSlotPref);
	lua_register(lua, "GetPlayerWeaponID", GetPlayerWeaponID);
	lua_register(lua, "GetWeaponID", GetWeaponID);
	lua_register(lua, "GetEntityWeaponID", GetEntityWeaponID);
	lua_register(lua, "GetWeaponName", GetWeaponName);
	lua_register(lua, "SetWeaponDamage", SetWeaponDamage);
	lua_register(lua, "SetWeaponAccuracy", SetWeaponAccuracy);
	lua_register(lua, "SetWeaponReloadQuantity", SetWeaponReloadQuantity);
	lua_register(lua, "SetWeaponFireIterations", SetWeaponFireIterations);
	lua_register(lua, "SetWeaponRange", SetWeaponRange);
	lua_register(lua, "SetWeaponDropoff", SetWeaponDropoff);
	lua_register(lua, "SetWeaponSpotLighting", SetWeaponSpotLighting);
	lua_register(lua, "GetWeaponDamage", GetWeaponDamage);
	lua_register(lua, "GetWeaponAccuracy", GetWeaponAccuracy);
	lua_register(lua, "GetWeaponReloadQuantity", GetWeaponReloadQuantity);
	lua_register(lua, "GetWeaponFireIterations", GetWeaponFireIterations);
	lua_register(lua, "GetWeaponRange", GetWeaponRange);
	lua_register(lua, "GetWeaponDropoff", GetWeaponDropoff);
	lua_register(lua, "GetWeaponSpotLighting", GetWeaponSpotLighting);
	lua_register(lua, "SetWeaponFireRate", SetWeaponFireRate);
	lua_register(lua, "GetWeaponFireRate", GetWeaponFireRate);
	lua_register(lua, "SetWeaponClipCapacity", SetWeaponClipCapacity);
	lua_register(lua, "GetWeaponClipCapacity", GetWeaponClipCapacity);

	lua_register(lua, "WrapAngle", WrapAngle);
	lua_register(lua, "GetCameraOverride", GetCameraOverride);
	lua_register(lua, "SetCameraOverride", SetCameraOverride);
	lua_register(lua, "SetCameraFOV", SetCameraFOV);
	lua_register(lua, "SetCameraPosition", SetCameraPosition);
	lua_register(lua, "SetCameraAngle", SetCameraAngle);
	lua_register(lua, "SetCameraFreeFlight", SetCameraFreeFlight);
	lua_register(lua, "GetCameraPositionX", GetCameraPositionX);
	lua_register(lua, "GetCameraPositionY", GetCameraPositionY);
	lua_register(lua, "GetCameraPositionZ", GetCameraPositionZ);
	lua_register(lua, "GetCameraAngleX", GetCameraAngleX);
	lua_register(lua, "GetCameraAngleY", GetCameraAngleY);
	lua_register(lua, "GetCameraAngleZ", GetCameraAngleZ);
	lua_register(lua, "GetCameraLookAtX", GetCameraLookAtX);
	lua_register(lua, "GetCameraLookAtY", GetCameraLookAtY);
	lua_register(lua, "GetCameraLookAtZ", GetCameraLookAtZ);

	lua_register(lua, "ForcePlayer", ForcePlayer);

	lua_register(lua, "SetEntityActive", SetEntityActive);
	lua_register(lua, "SetEntityActivated", SetEntityActivated);
	lua_register(lua, "SetEntityObjective", SetEntityObjective);
	lua_register(lua, "SetEntityCollectable", SetEntityCollectable);
	lua_register(lua, "SetEntityCollected", SetEntityCollected);
	lua_register(lua, "SetEntityCollectedForce", SetEntityCollectedForce);
	lua_register(lua, "SetEntityUsed", SetEntityUsed);
	lua_register(lua, "GetEntityObjective", GetEntityObjective);
	lua_register(lua, "GetEntityExplodable", GetEntityExplodable);
	lua_register(lua, "SetEntityExplodable", SetEntityExplodable);
	lua_register(lua, "SetExplosionHeight", SetExplosionHeight);
	lua_register(lua, "SetExplosionDamage", SetExplosionDamage);
	lua_register(lua, "SetCustomExplosion", SetCustomExplosion);
	
	
	lua_register(lua, "GetEntityProjectGlobal", GetEntityProjectGlobal);
	lua_register(lua, "GetEntityCollectable", GetEntityCollectable);
	lua_register(lua, "GetEntityCollected", GetEntityCollected);
	lua_register(lua, "GetEntityUsed", GetEntityUsed);
	lua_register(lua, "SetEntityQuantity", SetEntityQuantity);
	lua_register(lua, "GetEntityQuantity", GetEntityQuantity);
	lua_register(lua, "SetEntityHasKey", SetEntityHasKey);
	lua_register(lua, "GetEntityActive", GetEntityActive);
	lua_register(lua, "GetEntityWhoActivated", GetEntityWhoActivated);
	lua_register(lua, "GetEntityVisibility", GetEntityVisibility);
	lua_register(lua, "GetEntityPositionX", GetEntityPositionX);
	lua_register(lua, "GetEntityPositionY", GetEntityPositionY);
	lua_register(lua, "GetEntityPositionZ", GetEntityPositionZ);
	lua_register(lua, "GetEntityCollBox", GetEntityCollBox);
	lua_register(lua, "GetEntityColBox", GetEntityColBox);
	lua_register(lua, "GetEntityPosAng", GetEntityPosAng);
	lua_register(lua, "GetEntityWeight", GetEntityWeight);
	lua_register(lua, "GetEntityScales", GetEntityScales);
	lua_register(lua, "GetEntityName", GetEntityName);
	lua_register(lua, "GetEntityAngleX", GetEntityAngleX);
	lua_register(lua, "GetEntityAngleY", GetEntityAngleY);
	lua_register(lua, "GetEntityAngleZ", GetEntityAngleZ);
	lua_register(lua, "GetMovementSpeed", GetMovementSpeed);
	lua_register(lua, "GetAnimationSpeed", GetAnimationSpeed);
	lua_register(lua, "SetAnimationSpeedModulation", SetAnimationSpeedModulation);
	lua_register(lua, "GetAnimationSpeedModulation", GetAnimationSpeedModulation);
	lua_register(lua, "GetMovementDelta", GetMovementDelta);
	lua_register(lua, "GetMovementDeltaManually", GetMovementDeltaManually);
	lua_register(lua, "GetEntityMarkerMode", GetEntityMarkerMode);
	lua_register(lua, "SetEntityAlwaysActive", SetEntityAlwaysActive);
	lua_register(lua, "GetEntityAlwaysActive", GetEntityAlwaysActive);
	lua_register(lua, "SetEntityUnderwaterMode", SetEntityUnderwaterMode);
	lua_register(lua, "GetEntityUnderwaterMode", GetEntityUnderwaterMode);
	lua_register(lua, "GetEntityParentID", GetEntityParentID);

	lua_register(lua, "SetEntityIfUsed", SetEntityIfUsed);
	lua_register(lua, "GetEntityIfUsed", GetEntityIfUsed);

	#ifdef WICKEDENGINE
	lua_register(lua, "SetEntityAllegiance", SetEntityAllegiance);
	lua_register(lua, "GetEntityAllegiance", GetEntityAllegiance);
	lua_register(lua, "GetEntityPatrolMode", GetEntityPatrolMode);
	lua_register(lua, "GetEntityRelationshipID", GetEntityRelationshipID);
	lua_register(lua, "GetEntityRelationshipType", GetEntityRelationshipType);
	lua_register(lua, "GetEntityRelationshipData", GetEntityRelationshipData);
	lua_register(lua, "GetEntityCanFire", GetEntityCanFire);
	lua_register(lua, "GetEntityHasWeapon", GetEntityHasWeapon);
	lua_register(lua, "GetEntityViewAngle", GetEntityViewAngle);
	lua_register(lua, "GetEntityViewRange", GetEntityViewRange);
	lua_register(lua, "SetEntityViewRange", SetEntityViewRange);
	lua_register(lua, "GetEntityMoveSpeed", GetEntityMoveSpeed);
	lua_register(lua, "GetEntityTurnSpeed", GetEntityTurnSpeed);
	lua_register(lua, "SetEntityMoveSpeed", SetEntityMoveSpeed);
	lua_register(lua, "SetEntityTurnSpeed", SetEntityTurnSpeed);
	#endif

	#ifdef WICKEDENGINE
	lua_register(lua, "GetEntitiesWithinCone", GetEntitiesWithinCone);
	lua_register(lua, "GetEntityWithinCone", GetEntityWithinCone);
	#endif

	#ifdef WICKEDENGINE
	lua_register(lua, "GetNearestEntityDestroyed", GetNearestEntityDestroyed);
	lua_register(lua, "GetNearestSoundDistance", GetNearestSoundDistance);
	lua_register(lua, "MakeAISound", MakeAISound);
	#endif
	
	#ifdef WICKEDENGINE
	lua_register(lua, "GetTerrainEditableArea", GetTerrainEditableArea);
	#endif

	lua_register(lua, "SetEntityString", SetEntityString);
	lua_register(lua, "GetEntityString", GetEntityString);

	lua_register(lua, "GetLimbName", GetLimbName);
	lua_register(lua, "GetEntityLimbCount", GetEntityLimbCount);
	lua_register(lua, "GetEntityLimbName", GetEntityLimbName);
	lua_register(lua, "SetEntityLimbRotation", SetEntityLimbRotation);
	lua_register(lua, "GetEntityLimbRotation", GetEntityLimbRotation);
	lua_register(lua, "SetEntityLimbOffset", SetEntityLimbOffset);
	lua_register(lua, "GetEntityLimbOffset", GetEntityLimbOffset);
	lua_register(lua, "SetEntityLimbPivot", SetEntityLimbPivot);
	lua_register(lua, "GetEntityLimbPivot", GetEntityLimbPivot);
	lua_register(lua, "GetEntityLimbBounds", GetEntityLimbBounds);
	lua_register(lua, "GetEntityLimbParent", GetEntityLimbParent);
	lua_register(lua, "GetEntityLimbBoneCount", GetEntityLimbBoneCount);
	lua_register(lua, "GetEntityLimbBone", GetEntityLimbBone);
	lua_register(lua, "SetEntityLimbAlpha", SetEntityLimbAlpha);
	lua_register(lua, "GetEntityLimbAlpha", GetEntityLimbAlpha);

	lua_register(lua, "SetEntitySpawnAtStart", SetEntitySpawnAtStart);
	lua_register(lua, "GetEntitySpawnAtStart", GetEntitySpawnAtStart);
	lua_register(lua, "GetEntityFilePath", GetEntityFilePath);
	lua_register(lua, "GetEntityClonedSinceStartValue", GetEntityClonedSinceStartValue);
	lua_register(lua, "SetPreExitValue", SetPreExitValue);

	lua_register(lua, "SetEntityAnimation", SetEntityAnimation);
	lua_register(lua, "GetEntityAnimationStart", GetEntityAnimationStart);
	lua_register(lua, "GetEntityAnimationFinish", GetEntityAnimationFinish);
	lua_register(lua, "GetEntityAnimationFound", GetEntityAnimationFound);
	lua_register(lua, "GetObjectAnimationFinished", GetObjectAnimationFinished);

	lua_register(lua, "AdjustLookSettingHorizLimit", AdjustLookSettingHorizLimit);
	lua_register(lua, "AdjustLookSettingHorizOffset", AdjustLookSettingHorizOffset);
	lua_register(lua, "AdjustLookSettingVertLimit", AdjustLookSettingVertLimit);
	lua_register(lua, "AdjustLookSettingVertOffset", AdjustLookSettingVertOffset);
	lua_register(lua, "AdjustAimSettingHorizLimit", AdjustAimSettingHorizLimit);
	lua_register(lua, "AdjustAimSettingHorizOffset", AdjustAimSettingHorizOffset);
	lua_register(lua, "AdjustAimSettingVertLimit", AdjustAimSettingVertLimit);
	lua_register(lua, "AdjustAimSettingVertOffset", AdjustAimSettingVertOffset);

	lua_register(lua, "GetEntityFootfallMax", GetEntityFootfallMax);
	lua_register(lua, "GetEntityFootfallKeyframe", GetEntityFootfallKeyframe);
	lua_register(lua, "GetEntityAnimationNameExist", GetEntityAnimationNameExist);
	lua_register(lua, "GetEntityAnimationNameExistAndPlaying", GetEntityAnimationNameExistAndPlaying);
	lua_register(lua, "GetEntityAnimationNameExistAndPlayingSearchAny", GetEntityAnimationNameExistAndPlayingSearchAny);
	lua_register(lua, "GetEntityAnimationTriggerFrame", GetEntityAnimationTriggerFrame);
	lua_register(lua, "GetEntityAnimationStartFinish", GetEntityAnimationStartFinish);
	
	// Entity creation/destruction
	lua_register(lua, "GetOriginalEntityElementMax", GetOriginalEntityElementMax);
	lua_register(lua, "CreateEntityIfNotPresent", CreateEntityIfNotPresent);
	lua_register(lua, "SpawnNewEntity", SpawnNewEntity);
	lua_register(lua, "DeleteNewEntity", DeleteNewEntity);

	lua_register(lua, "GetAmmoClipMax", GetAmmoClipMax);
	lua_register(lua, "GetAmmoClip", GetAmmoClip);
	lua_register(lua, "SetAmmoClip", SetAmmoClip);

	// Entity Physics
	lua_register(lua, "FreezeEntity", FreezeEntity);
	lua_register(lua, "UnFreezeEntity", UnFreezeEntity);

	// Terrain
	lua_register(lua, "GetTerrainHeight", GetTerrainHeight);
	lua_register(lua, "GetSurfaceHeight", GetSurfaceHeight);
	lua_register(lua, "SetPlayerSlopeAngle", SetPlayerSlopeAngle);
	lua_register(lua, "GetTerrainHeightFloat", GetTerrainHeightFloat);

	// DarkAI - Legacy Automatic Mode
	lua_register(lua, "AIEntityAssignPatrolPath" , AIEntityAssignPatrolPath );
	lua_register(lua, "AIEntityAddTarget" , AIEntityAddTarget );
	lua_register(lua, "AIEntityRemoveTarget" , AIEntityRemoveTarget );
	lua_register(lua, "AIEntityMoveToCover" , AIEntityMoveToCover );
	lua_register(lua, "AIGetEntityCanFire" , AIGetEntityCanFire );

	// DarkAI - New Manual Mode Control
	lua_register(lua, "AISetEntityControl", AISetEntityControl);
	lua_register(lua, "AIEntityStop" , AIEntityStop );
	lua_register(lua, "AIGetEntityCanSee" , AIGetEntityCanSee );
	lua_register(lua, "AIGetEntityViewRange" , AIGetEntityViewRange );
	lua_register(lua, "AIGetEntitySpeed" , AIGetEntitySpeed );
	lua_register(lua, "AISetEntityMoveBoostPriority" , AISetEntityMoveBoostPriority );
	lua_register(lua, "AIEntityGoToPosition" , AIEntityGoToPosition );
	lua_register(lua, "AIGetEntityHeardSound" , AIGetEntityHeardSound );
	lua_register(lua, "AISetEntityPosition" , AISetEntityPosition );
	lua_register(lua, "AISetEntityTurnSpeed" , AISetEntityTurnSpeed );
	lua_register(lua, "AIGetEntityAngleY" , AIGetEntityAngleY );
	lua_register(lua, "AIGetEntityIsMoving" , AIGetEntityIsMoving );

	lua_register(lua, "AIGetTotalPaths" , AIGetTotalPaths );
	lua_register(lua, "AIGetPathCountPoints" , AIGetPathCountPoints );
	lua_register(lua, "AIPathGetPointX" , AIPathGetPointX );
	lua_register(lua, "AIPathGetPointY" , AIPathGetPointY );
	lua_register(lua, "AIPathGetPointZ" , AIPathGetPointZ );

	lua_register(lua, "AIGetTotalCover" , AIGetTotalCover );
	lua_register(lua, "AICoverGetPointX" , AICoverGetPointX );
	lua_register(lua, "AICoverGetPointY" , AICoverGetPointY );
	lua_register(lua, "AICoverGetPointZ" , AICoverGetPointZ );
	lua_register(lua, "AICoverGetAngle" , AICoverGetAngle );
	lua_register(lua, "AICoverGetIfUsed" , AICoverGetIfUsed );
	lua_register(lua, "AICouldSee", AICouldSee);

	// New Attachment Commands
	#ifdef WICKEDENGINE
	lua_register(lua, "HideEntityAttachment", HideEntityAttachment);
	lua_register(lua, "ShowEntityAttachment", ShowEntityAttachment);
	#endif

	// New Debugging Commands
	#ifdef WICKEDENGINE
	lua_register(lua, "SetDebuggingData", SetDebuggingData);
	#endif
		
	// New RecastDetour (RD) AI Commands
	lua_register(lua, "RDFindPath", RDFindPath);
	lua_register(lua, "RDGetPathPointCount", RDGetPathPointCount);
	lua_register(lua, "RDGetPathPointX", RDGetPathPointX);
	lua_register(lua, "RDGetPathPointY", RDGetPathPointY);
	lua_register(lua, "RDGetPathPointZ", RDGetPathPointZ);
	lua_register(lua, "StartMoveAndRotateToXYZ", StartMoveAndRotateToXYZ);
	lua_register(lua, "MoveAndRotateToXYZ", MoveAndRotateToXYZ);
	lua_register(lua, "SetEntityPathRotationMode", SetEntityPathRotationMode);	
	lua_register(lua, "RDIsWithinMesh", RDIsWithinMesh);
	lua_register(lua, "RDIsWithinAndOverMesh", RDIsWithinAndOverMesh);
	lua_register(lua, "RDGetYFromMeshPosition", RDGetYFromMeshPosition);
	lua_register(lua, "RDBlockNavMesh", RDBlockNavMesh);
	lua_register(lua, "RDBlockNavMeshWithShape", RDBlockNavMeshWithShape);

	lua_register(lua, "DoTokenDrop", DoTokenDrop);
	lua_register(lua, "GetTokenDropCount", GetTokenDropCount);
	lua_register(lua, "GetTokenDropX", GetTokenDropZ);
	lua_register(lua, "GetTokenDropY", GetTokenDropY);
	lua_register(lua, "GetTokenDropZ", GetTokenDropZ);
	lua_register(lua, "GetTokenDropType", GetTokenDropType);
	lua_register(lua, "GetTokenDropTimeLeft", GetTokenDropTimeLeft);

	lua_register(lua, "AdjustPositionToGetLineOfSight", AdjustPositionToGetLineOfSight);
	lua_register(lua, "SetCharacterMode", SetCharacterMode);
	
	// Visual Attribs
	lua_register(lua, "GetFogNearest" , GetFogNearest );
	lua_register(lua, "GetFogDistance" , GetFogDistance );
	lua_register(lua, "GetFogRed" , GetFogRed );
	lua_register(lua, "GetFogGreen" , GetFogGreen );
	lua_register(lua, "GetFogBlue" , GetFogBlue );
	lua_register(lua, "GetFogIntensity" , GetFogIntensity );
	lua_register(lua, "GetAmbienceIntensity" , GetAmbienceIntensity );
	lua_register(lua, "GetAmbienceRed" , GetAmbienceRed );
	lua_register(lua, "GetAmbienceGreen" , GetAmbienceGreen );
	lua_register(lua, "GetAmbienceBlue" , GetAmbienceBlue );
	lua_register(lua, "GetSurfaceRed" , GetSurfaceRed );
	lua_register(lua, "GetSurfaceGreen" , GetSurfaceGreen );
	lua_register(lua, "GetSurfaceBlue" , GetSurfaceBlue );
	lua_register(lua, "GetSurfaceIntensity" , GetSurfaceIntensity );
	lua_register(lua, "GetPostVignetteRadius" , GetPostVignetteRadius );
	lua_register(lua, "GetPostVignetteIntensity" , GetPostVignetteIntensity );
	lua_register(lua, "GetPostMotionDistance" , GetPostMotionDistance );
	lua_register(lua, "GetPostMotionIntensity" , GetPostMotionIntensity );
	lua_register(lua, "GetPostDepthOfFieldDistance" , GetPostDepthOfFieldDistance );
	lua_register(lua, "GetPostDepthOfFieldIntensity" , GetPostDepthOfFieldIntensity );

	lua_register(lua, "LoadImage" , LoadImage );
	lua_register(lua, "DeleteImage" , DeleteSpriteImage );
	lua_register(lua, "GetImageWidth" , GetImageWidth );
	lua_register(lua, "GetImageHeight" , GetImageHeight );
	lua_register(lua, "CreateSprite" , CreateSprite );
	lua_register(lua, "PasteSprite" , PasteSprite );
	lua_register(lua, "PasteSpritePosition" , PasteSpritePosition );
	lua_register(lua, "SetSpritePosition", SetSpritePosition);
	lua_register(lua, "SetSpritePriority" , SetSpritePriorityForLUA);
	lua_register(lua, "SetSpriteSize" , SetSpriteSize );
	lua_register(lua, "SetSpriteDepth" , SetSpriteDepth );
	lua_register(lua, "SetSpriteColor" , SetSpriteColor );	
	lua_register(lua, "SetSpriteAngle" , SetSpriteAngle );	
	lua_register(lua, "SetSpriteOffset" , SetSpriteOffset );
	lua_register(lua, "DeleteSprite" , DeleteSprite );
	lua_register(lua, "SetSpriteScissor", SetSpriteScissor);
	lua_register(lua, "SetSpriteImage", SetSpriteImage);
	lua_register(lua, "DrawSpritesFirst" , DrawSpritesFirstForLUA );
	lua_register(lua, "DrawSpritesLast" , DrawSpritesLastForLUA );	
	lua_register(lua, "BackdropOff" , BackdropOffForLUA );	
	lua_register(lua, "BackdropOn" , BackdropOnForLUA );	

	lua_register(lua, "LoadGlobalSound" , LoadGlobalSound );	
	lua_register(lua, "PlayGlobalSound" , PlayGlobalSound );	
	lua_register(lua, "LoopGlobalSound" , LoopGlobalSound );	
	lua_register(lua, "StopGlobalSound" , StopGlobalSound );	
	lua_register(lua, "DeleteGlobalSound" , DeleteGlobalSound );	
	lua_register(lua, "SetGlobalSoundSpeed" , SetGlobalSoundSpeed );	
	lua_register(lua, "SetGlobalSoundVolume" , SetGlobalSoundVolume );	
	lua_register(lua, "GetGlobalSoundExist" , GetGlobalSoundExist );
	lua_register(lua, "GetGlobalSoundPlaying" , GetGlobalSoundPlaying );
	lua_register(lua, "GetGlobalSoundLooping" , GetGlobalSoundLooping );
	lua_register(lua, "GetSoundPlaying", GetSoundPlaying);

	lua_register(lua, "PlayRawSound" , PlayRawSound );
	lua_register(lua, "LoopRawSound" , LoopRawSound );
	lua_register(lua, "StopRawSound" , StopRawSound );
	lua_register(lua, "SetRawSoundVolume" , SetRawSoundVolume );
	lua_register(lua, "SetRawSoundSpeed" , SetRawSoundSpeed );
	lua_register(lua, "SetRawSoundDistanceScale" , SetRawSoundDistanceScale );
	lua_register(lua, "GetRawSoundDistanceScale" , GetRawSoundDistanceScale );
	lua_register(lua, "SetDopplerScale" , SetDopplerScale );
	lua_register(lua, "GetDopplerScale" , GetDopplerScale );
	lua_register(lua, "RawSoundExist" , RawSoundExist );
	lua_register(lua, "RawSoundPlaying" , RawSoundPlaying );
	lua_register(lua, "GetEntityRawSound" , GetEntityRawSound );
	lua_register(lua, "StartAmbientMusicTrack", StartAmbientMusicTrack);
	lua_register(lua, "StopAmbientMusicTrack", StopAmbientMusicTrack);
	lua_register(lua, "SetAmbientMusicTrackVolume", SetAmbientMusicTrackVolume);
	lua_register(lua, "StartCombatMusicTrack", StartCombatMusicTrack);
	lua_register(lua, "StopCombatMusicTrack", StopCombatMusicTrack);
	lua_register(lua, "SetCombatMusicTrackVolume", SetCombatMusicTrackVolume);
	lua_register(lua, "GetCombatMusicTrackPlaying", GetCombatMusicTrackPlaying);

	lua_register(lua, "SetSoundMusicMode", SetSoundMusicMode);
	lua_register(lua, "GetSoundMusicMode", GetSoundMusicMode);

	#ifdef VRTECH
	lua_register(lua, "GetSpeech" , GetSpeech );
	#endif

	lua_register(lua, "GetTimeElapsed" , GetTimeElapsed );
	lua_register(lua, "GetKeyState" , GetKeyState );
	lua_register(lua, "SetGlobalTimer" , SetGlobalTimer);
	lua_register(lua, "Timer", GetGlobalTimer);
	lua_register(lua, "MouseMoveX" , MouseMoveX );
	lua_register(lua, "MouseMoveY" , MouseMoveY );
	lua_register(lua, "GetDesktopWidth" , GetDesktopWidth );
	lua_register(lua, "GetDesktopHeight" , GetDesktopHeight );
	lua_register(lua, "CurveValue" , CurveValue );
	lua_register(lua, "CurveAngle" , CurveAngle );
	lua_register(lua, "PositionMouse" , PositionMouse );
	lua_register(lua, "GetDynamicCharacterControllerDidJump" , GetDynamicCharacterControllerDidJump );
	lua_register(lua, "GetCharacterControllerDucking" , GetCharacterControllerDucking );
	lua_register(lua, "WrapValue" , WrapValue );
	lua_register(lua, "GetElapsedTime" , GetElapsedTime );
	lua_register(lua, "SetGamePaused" , SetGamePaused );
	lua_register(lua, "GetGamePaused" , GetGamePaused );
	lua_register(lua, "SetEscapeMenu" , SetEscapeMenu );
	lua_register(lua, "GetVideoMemoryUsed" , GetVideoMemoryUsed );
	lua_register(lua, "GetDrawCalls" , GetDrawCalls );
	lua_register(lua, "SetRenderTiming" , SetRenderTiming );
	lua_register(lua, "GetRenderTime" , GetRenderTime );
	lua_register(lua, "GetPlrObjectPositionX" , GetPlrObjectPositionX );
	lua_register(lua, "GetPlrObjectPositionY" , GetPlrObjectPositionY );
	lua_register(lua, "GetPlrObjectPositionZ" , GetPlrObjectPositionZ );
	lua_register(lua, "GetPlrObjectAngleX" , GetPlrObjectAngleX );
	lua_register(lua, "GetPlrObjectAngleY" , GetPlrObjectAngleY );
	lua_register(lua, "GetPlrObjectAngleZ" , GetPlrObjectAngleZ );
	lua_register(lua, "GetGroundHeight" , GetGroundHeight );
	lua_register(lua, "NewXValue" , NewXValue );
	lua_register(lua, "NewZValue" , NewZValue );
	lua_register(lua, "ControlDynamicCharacterController" , ControlDynamicCharacterController );
	lua_register(lua, "GetCharacterHitFloor" , GetCharacterHitFloor );
	lua_register(lua, "GetCharacterFallDistance" , GetCharacterFallDistance );
	lua_register(lua, "RayTerrain" , RayTerrain );
	lua_register(lua, "GetRayCollisionX" , GetRayCollisionX );
	lua_register(lua, "GetRayCollisionY" , GetRayCollisionY );
	lua_register(lua, "GetRayCollisionZ" , GetRayCollisionZ );
	lua_register(lua, "GetRayNormalX" , GetRayNormalX );
	lua_register(lua, "GetRayNormalY" , GetRayNormalY );
	lua_register(lua, "GetRayNormalZ" , GetRayNormalZ );
	lua_register(lua, "IntersectAll" , IntersectAll );
	lua_register(lua, "IntersectRay" , IntersectRay );
	lua_register(lua, "RayRegionBegin", RayRegionBegin);
	lua_register(lua, "RayRegion", RayRegion);
	lua_register(lua, "RayRegionEnd", RayRegionEnd);
	lua_register(lua, "IntersectStatic", IntersectStatic);
	lua_register(lua, "IntersectStaticPerformant", IntersectStaticPerformant);
	lua_register(lua, "IntersectAllIncludeTerrain", IntersectAllIncludeTerrain);
	lua_register(lua, "GetIntersectCollisionX" , GetIntersectCollisionX );
	lua_register(lua, "GetIntersectCollisionY" , GetIntersectCollisionY );
	lua_register(lua, "GetIntersectCollisionZ" , GetIntersectCollisionZ );
	lua_register(lua, "GetIntersectCollisionNX" , GetIntersectCollisionNX );
	lua_register(lua, "GetIntersectCollisionNY" , GetIntersectCollisionNY );
	lua_register(lua, "GetIntersectCollisionNZ" , GetIntersectCollisionNZ );
	lua_register(lua, "GetIntersectCollisionLimb" , GetIntersectCollisionLimb );
	lua_register(lua, "IntersectGetLastHitBone", IntersectGetLastHitBone);
	lua_register(lua, "IntersectGetLastHitFrame", IntersectGetLastHitFrame);

	lua_register(lua, "PositionCamera" , PositionCamera );
	lua_register(lua, "PointCamera" , PointCamera );
	lua_register(lua, "MoveCamera" , MoveCamera );
	lua_register(lua, "GetObjectExist" , GetObjectExist );
	lua_register(lua, "SetObjectFrame", SetObjectFrame);
	lua_register(lua, "GetObjectFrame" , GetObjectFrame );
	lua_register(lua, "SetObjectSpeed" , SetObjectSpeed );
	lua_register(lua, "GetObjectSpeed" , GetObjectSpeed );
	lua_register(lua, "PositionObject" , PositionObject );
	lua_register(lua, "RotateObject" , RotateObject );
	lua_register(lua, "GetObjectAngleX" , GetObjectAngleX );
	lua_register(lua, "GetObjectAngleY" , GetObjectAngleY );
	lua_register(lua, "GetObjectAngleZ" , GetObjectAngleZ );
	lua_register(lua, "GetObjectPosAng",  GetObjectPosAng );
	lua_register(lua, "GetObjectColBox",  GetObjectColBox );
	lua_register(lua, "GetObjectCentre", GetObjectCentre);
	lua_register(lua, "GetObjectColCentre", GetObjectColCentre);
	lua_register(lua, "GetObjectScales",  GetObjectScales );
	lua_register(lua, "ScaleObject",   ScaleObjectXYZ );

	// Physics related functions
	lua_register(lua, "PushObject",              PushObject );
	lua_register(lua, "ConstrainObjMotion",      ConstrainObjMotion );
	lua_register(lua, "ConstrainObjRotation",    ConstrainObjRotation );
	lua_register(lua, "CreateSingleHinge",       CreateSingleHinge );
	lua_register(lua, "CreateSingleJoint",       CreateSingleJoint );
	lua_register(lua, "CreateDoubleHinge",       CreateDoubleHinge );
	lua_register(lua, "CreateDoubleJoint",       CreateDoubleJoint );
	lua_register(lua, "CreateSliderDouble",      CreateSliderDouble );
	lua_register(lua, "RemoveObjectConstraints", RemoveObjectConstraints );
	lua_register(lua, "RemoveConstraint",        RemoveConstraint );
	lua_register(lua, "PhysicsRayCast",          PhysicsRayCast );
	lua_register(lua, "PhysicsRay", PhysicsRay);
	lua_register(lua, "PhysicsSweepBox", PhysicsSweepBox);
	lua_register(lua, "PhysicsOverlapBox", PhysicsOverlapBox);
	lua_register(lua, "GetPhysicsStats", GetPhysicsStats);
	lua_register(lua, "GetPhysicsBodyPose", GetPhysicsBodyPose);
	lua_register(lua, "SetObjectDamping",        SetObjectDamping );
	lua_register(lua, "SetHingeLimits",          SetHingeLimits );
	lua_register(lua, "GetHingeAngle",           GetHingeAngle );
	lua_register(lua, "SetHingeMotor",           SetHingeMotor );
	lua_register(lua, "SetSliderMotor",          SetSliderMotor );
	lua_register(lua, "SetBodyScaling",          SetBodyScaling );
	lua_register(lua, "SetSliderLimits",         SetSliderLimits );
	lua_register(lua, "GetSliderPosition",       GetSliderPosition );

	// Collision detection functions 
	lua_register(lua, "GetObjectNumCollisions",     GetObjectNumCollisions );
	lua_register(lua, "GetObjectCollisionDetails",  GetObjectCollisionDetails );
	lua_register(lua, "GetTerrainNumCollisions",    GetTerrainNumCollisions );
	lua_register(lua, "GetTerrainCollisionDetails", GetTerrainCollisionDetails );
	lua_register(lua, "AddObjectCollisionCheck",    AddObjectCollisionCheck );
	lua_register(lua, "RemoveObjectCollisionCheck", RemoveObjectCollisionCheck );

	// quaternion library functions
	lua_register(lua, "QuatToEuler",  QuatToEuler );
	lua_register(lua, "EulerToQuat",  EulerToQuat );
	lua_register(lua, "AnglesToQuat", AnglesToQuat );
	lua_register(lua, "QuatToAngles", QuatToAngles );
	lua_register(lua, "QuatMultiply", QuatMultiply );
	lua_register(lua, "QuatSLERP",    QuatSLERP );
	lua_register(lua, "QuatLERP",     QuatLERP );

	lua_register(lua, "Convert3DTo2D", LuaConvert3DTo2D); // x,y = Convert3DTo2D(x,y,z)
	lua_register(lua, "Convert2DTo3D", LuaConvert2DTo3D); // px,py,pz,dx,dy,dz = Convert2DTo3D(x percent,y percent) -- percent
	lua_register(lua, "ScreenCoordsToPercent", ScreenCoordsToPercent); // percentx,percentx = ScreenCordsToPercent(x,y) -- return percent positions.

	
	// Lua control of dynamic light
	lua_register(lua, "GetEntityLightNumber", GetEntityLightNumber );
	lua_register(lua, "GetLightPosition",     GetLightPosition );
	lua_register(lua, "GetLightAngle",		  GetLightAngle);
	lua_register(lua, "GetLightEuler",		  GetLightEuler);
	lua_register(lua, "GetLightRGB",          GetLightRGB );
	lua_register(lua, "GetLightRange",        GetLightRange );
	lua_register(lua, "SetLightPosition",     SetLightPosition );
	lua_register(lua, "SetLightAngle",		  SetLightAngle);
	lua_register(lua, "SetLightEuler",		  SetLightEuler);
	lua_register(lua, "SetLightRGB",          SetLightRGB );
	lua_register(lua, "SetLightShadow", SetLightShadow);
	lua_register(lua, "SetLightRange",        SetLightRange );
	
	lua_register(lua, "RunCharLoop" , RunCharLoop );
	lua_register(lua, "TriggerWaterRipple" , TriggerWaterRipple );
	lua_register(lua, "TriggerWaterRippleSize", TriggerWaterRippleSize);
	lua_register(lua, "SetDecalRange", SetDecalRange);
	lua_register(lua, "GetDecalRange", GetDecalRange);
	lua_register(lua, "SetDecalLimit", SetDecalLimit);
	lua_register(lua, "GetDecalLimit", GetDecalLimit);
	lua_register(lua, "GetDecalStats", GetDecalStats);
	lua_register(lua, "AddProjectedDecal", AddProjectedDecal);
	lua_register(lua, "AddBlastDecal", AddBlastDecal);
	lua_register(lua, "SetProjectedDecalBlend", SetProjectedDecalBlend);
	lua_register(lua, "RemoveProjectedDecal", RemoveProjectedDecal);
	lua_register(lua, "SetProjectedDecalOpacity", SetProjectedDecalOpacity);
	lua_register(lua, "SetProjectedDecalFacing", SetProjectedDecalFacing);
	lua_register(lua, "SetProjectedDecalGrass", SetProjectedDecalGrass);
	lua_register(lua, "SetProjectedDecalLimit", SetProjectedDecalLimit);
	lua_register(lua, "GetProjectedDecalLimit", GetProjectedDecalLimit);
	lua_register(lua, "GetProjectedDecalCount", GetProjectedDecalCount);
	lua_register(lua, "SetEntityReceivesDecals", SetEntityReceivesDecals);
	lua_register(lua, "SetEntityLOD", SetEntityLOD);
	lua_register(lua, "SetEntityShaderParam", SetEntityShaderParam);
	lua_register(lua, "SetEntityTextureScroll", SetEntityTextureScroll);
	lua_register(lua, "TriggerWaterSplash", TriggerWaterSplash);
	lua_register(lua, "PlayFootfallSound" , PlayFootfallSound );
	lua_register(lua, "ResetUnderwaterState" , ResetUnderwaterState );
	lua_register(lua, "SetUnderwaterOn" , SetUnderwaterOn );
	lua_register(lua, "SetUnderwaterOff" , SetUnderwaterOff );

	lua_register(lua, "SetWorldGravity", SetWorldGravity);

	lua_register(lua, "SetShaderVariable" , SetShaderVariable );
	
	lua_register(lua, "GetGamePlayerControlJetpackMode" , GetGamePlayerControlJetpackMode );
	lua_register(lua, "GetGamePlayerControlJetpackFuel" , GetGamePlayerControlJetpackFuel );
	lua_register(lua, "GetGamePlayerControlJetpackHidden" , GetGamePlayerControlJetpackHidden );
	lua_register(lua, "GetGamePlayerControlJetpackCollected" , GetGamePlayerControlJetpackCollected );
	lua_register(lua, "GetGamePlayerControlSoundStartIndex" , GetGamePlayerControlSoundStartIndex );
	lua_register(lua, "GetGamePlayerControlJetpackParticleEmitterIndex" , GetGamePlayerControlJetpackParticleEmitterIndex );
	lua_register(lua, "GetGamePlayerControlJetpackThrust" , GetGamePlayerControlJetpackThrust );
	lua_register(lua, "GetGamePlayerControlStartStrength" , GetGamePlayerControlStartStrength );
	lua_register(lua, "GetGamePlayerControlIsRunning" , GetGamePlayerControlIsRunning );
	lua_register(lua, "GetGamePlayerControlFinalCameraAngley" , GetGamePlayerControlFinalCameraAngley );
	lua_register(lua, "GetGamePlayerControlCx" , GetGamePlayerControlCx );
	lua_register(lua, "GetGamePlayerControlCy" , GetGamePlayerControlCy );
	lua_register(lua, "GetGamePlayerControlCz" , GetGamePlayerControlCz );
	lua_register(lua, "GetGamePlayerControlBasespeed" , GetGamePlayerControlBasespeed );
	lua_register(lua, "GetGamePlayerControlCanRun" , GetGamePlayerControlCanRun );
	lua_register(lua, "GetGamePlayerControlMaxspeed" , GetGamePlayerControlMaxspeed );
	lua_register(lua, "GetGamePlayerControlTopspeed" , GetGamePlayerControlTopspeed );
	lua_register(lua, "GetGamePlayerControlMovement" , GetGamePlayerControlMovement );
	lua_register(lua, "GetGamePlayerControlMovey" , GetGamePlayerControlMovey );
	lua_register(lua, "GetGamePlayerControlLastMovement" , GetGamePlayerControlLastMovement );
	lua_register(lua, "GetGamePlayerControlFootfallCount" , GetGamePlayerControlFootfallCount );
	lua_register(lua, "GetGamePlayerControlLastMovement" , GetGamePlayerControlLastMovement );
	lua_register(lua, "GetGamePlayerControlGravityActive" , GetGamePlayerControlGravityActive );
	lua_register(lua, "GetGamePlayerControlPlrHitFloorMaterial" , GetGamePlayerControlPlrHitFloorMaterial );
	lua_register(lua, "GetGamePlayerControlUnderwater" , GetGamePlayerControlUnderwater );
	lua_register(lua, "GetGamePlayerControlJumpMode" , GetGamePlayerControlJumpMode );
	lua_register(lua, "GetGamePlayerControlJumpModeCanAffectVelocityCountdown" , GetGamePlayerControlJumpModeCanAffectVelocityCountdown );
	lua_register(lua, "GetGamePlayerControlSpeed" , GetGamePlayerControlSpeed );
	lua_register(lua, "GetGamePlayerControlAccel" , GetGamePlayerControlAccel );
	lua_register(lua, "GetGamePlayerControlSpeedRatio" , GetGamePlayerControlSpeedRatio );
	lua_register(lua, "GetGamePlayerControlWobble" , GetGamePlayerControlWobble );
	lua_register(lua, "GetGamePlayerControlWobbleSpeed" , GetGamePlayerControlWobbleSpeed );
	lua_register(lua, "GetGamePlayerControlWobbleHeight" , GetGamePlayerControlWobbleHeight );
	lua_register(lua, "GetGamePlayerControlJumpmax" , GetGamePlayerControlJumpmax );
	lua_register(lua, "GetGamePlayerControlPushangle" , GetGamePlayerControlPushangle );
	lua_register(lua, "GetGamePlayerControlPushforce" , GetGamePlayerControlPushforce );
	lua_register(lua, "GetGamePlayerControlFootfallPace" , GetGamePlayerControlFootfallPace );
	lua_register(lua, "GetGamePlayerControlFinalCameraAngley" , GetGamePlayerControlFinalCameraAngley );
	lua_register(lua, "GetGamePlayerControlLockAtHeight" , GetGamePlayerControlLockAtHeight );
	lua_register(lua, "GetGamePlayerControlControlHeight" , GetGamePlayerControlControlHeight );
	lua_register(lua, "GetGamePlayerControlControlHeightCooldown" , GetGamePlayerControlControlHeightCooldown );
	lua_register(lua, "GetGamePlayerControlStoreMovey" , GetGamePlayerControlStoreMovey );
	lua_register(lua, "GetGamePlayerControlPlrHitFloorMaterial" , GetGamePlayerControlPlrHitFloorMaterial );
	lua_register(lua, "GetGamePlayerControlHurtFall" , GetGamePlayerControlHurtFall );
	lua_register(lua, "GetGamePlayerControlLeanoverAngle" , GetGamePlayerControlLeanoverAngle );
	lua_register(lua, "GetGamePlayerControlLeanover" , GetGamePlayerControlLeanover );
	lua_register(lua, "GetGamePlayerControlCameraShake" , GetGamePlayerControlCameraShake );
	lua_register(lua, "GetGamePlayerControlFinalCameraAnglex" , GetGamePlayerControlFinalCameraAnglex );
	lua_register(lua, "GetGamePlayerControlFinalCameraAngley" , GetGamePlayerControlFinalCameraAngley );
	lua_register(lua, "GetGamePlayerControlFinalCameraAnglez" , GetGamePlayerControlFinalCameraAnglez );
	lua_register(lua, "GetGamePlayerControlCamRightMouseMode" , GetGamePlayerControlCamRightMouseMode );
	lua_register(lua, "GetGamePlayerControlCamCollisionSmooth" , GetGamePlayerControlCamCollisionSmooth );
	lua_register(lua, "GetGamePlayerControlCamCurrentDistance" , GetGamePlayerControlCamCurrentDistance );
	lua_register(lua, "GetGamePlayerControlCamDoFullRayCheck" , GetGamePlayerControlCamDoFullRayCheck );
	lua_register(lua, "GetGamePlayerControlLastGoodcx" , GetGamePlayerControlLastGoodcx );
	lua_register(lua, "GetGamePlayerControlLastGoodcy" , GetGamePlayerControlLastGoodcy );
	lua_register(lua, "GetGamePlayerControlLastGoodcz" , GetGamePlayerControlLastGoodcz );
	lua_register(lua, "GetGamePlayerControlCamDoFullRayCheck" , GetGamePlayerControlCamDoFullRayCheck );
	lua_register(lua, "GetGamePlayerControlFlinchx" , GetGamePlayerControlFlinchx );
	lua_register(lua, "GetGamePlayerControlFlinchy" , GetGamePlayerControlFlinchy );
	lua_register(lua, "GetGamePlayerControlFlinchz" , GetGamePlayerControlFlinchz );
	lua_register(lua, "GetGamePlayerControlFlinchCurrentx" , GetGamePlayerControlFlinchCurrentx );
	lua_register(lua, "GetGamePlayerControlFlinchCurrenty" , GetGamePlayerControlFlinchCurrenty );
	lua_register(lua, "GetGamePlayerControlFlinchCurrentz" , GetGamePlayerControlFlinchCurrentz );
	lua_register(lua, "GetGamePlayerControlFootfallType" , GetGamePlayerControlFootfallType );
	lua_register(lua, "GetGamePlayerControlRippleCount" , GetGamePlayerControlRippleCount );
	lua_register(lua, "GetGamePlayerControlLastFootfallSound" , GetGamePlayerControlLastFootfallSound );
	lua_register(lua, "GetGamePlayerControlInWaterState" , GetGamePlayerControlInWaterState );
	lua_register(lua, "GetGamePlayerControlDrownTimestamp" , GetGamePlayerControlDrownTimestamp );
	lua_register(lua, "GetGamePlayerControlDeadTime" , GetGamePlayerControlDeadTime );
	lua_register(lua, "GetGamePlayerControlSwimTimestamp" , GetGamePlayerControlSwimTimestamp );
	lua_register(lua, "GetGamePlayerControlRedDeathFog" , GetGamePlayerControlRedDeathFog );
	lua_register(lua, "GetGamePlayerControlHeartbeatTimeStamp" , GetGamePlayerControlHeartbeatTimeStamp );
	lua_register(lua, "GetGamePlayerControlThirdpersonEnabled" , GetGamePlayerControlThirdpersonEnabled );
	lua_register(lua, "GetGamePlayerControlThirdpersonCharacterIndex" , GetGamePlayerControlThirdpersonCharacterIndex );
	lua_register(lua, "GetGamePlayerControlThirdpersonCameraFollow" , GetGamePlayerControlThirdpersonCameraFollow );
	lua_register(lua, "GetGamePlayerControlThirdpersonCameraFocus" , GetGamePlayerControlThirdpersonCameraFocus );
	lua_register(lua, "GetGamePlayerControlThirdpersonCharactere" , GetGamePlayerControlThirdpersonCharactere );
	lua_register(lua, "GetGamePlayerControlThirdpersonCameraFollow" , GetGamePlayerControlThirdpersonCameraFollow );
	lua_register(lua, "GetGamePlayerControlThirdpersonShotFired" , GetGamePlayerControlThirdpersonShotFired );
	lua_register(lua, "GetGamePlayerControlThirdpersonCameraDistance" , GetGamePlayerControlThirdpersonCameraDistance );
	lua_register(lua, "GetGamePlayerControlThirdpersonCameraSpeed" , GetGamePlayerControlThirdpersonCameraSpeed );
	lua_register(lua, "GetGamePlayerControlThirdpersonCameraLocked" , GetGamePlayerControlThirdpersonCameraLocked );
	lua_register(lua, "GetGamePlayerControlThirdpersonCameraHeight" , GetGamePlayerControlThirdpersonCameraHeight );
	lua_register(lua, "GetGamePlayerControlThirdpersonCameraShoulder" , GetGamePlayerControlThirdpersonCameraShoulder );
	lua_register(lua, "GetGamePlayerControlFallDamageModifier", GetGamePlayerControlFallDamageModifier);
	lua_register(lua, "GetGamePlayerControlSwimSpeed", GetGamePlayerControlSwimSpeed);
	lua_register(lua, "SetGamePlayerControlJetpackMode" , SetGamePlayerControlJetpackMode );
	lua_register(lua, "SetGamePlayerControlJetpackFuel" , SetGamePlayerControlJetpackFuel );
	lua_register(lua, "SetGamePlayerControlJetpackHidden" , SetGamePlayerControlJetpackHidden );
	lua_register(lua, "SetGamePlayerControlJetpackCollected" , SetGamePlayerControlJetpackCollected );
	lua_register(lua, "SetGamePlayerControlSoundStartIndex" , SetGamePlayerControlSoundStartIndex );
	lua_register(lua, "SetGamePlayerControlJetpackParticleEmitterIndex" , SetGamePlayerControlJetpackParticleEmitterIndex );
	lua_register(lua, "SetGamePlayerControlJetpackThrust" , SetGamePlayerControlJetpackThrust );
	lua_register(lua, "SetGamePlayerControlStartStrength" , SetGamePlayerControlStartStrength );
	lua_register(lua, "SetGamePlayerControlIsRunning" , SetGamePlayerControlIsRunning );
	lua_register(lua, "SetGamePlayerControlFinalCameraAngley" , SetGamePlayerControlFinalCameraAngley );
	lua_register(lua, "SetGamePlayerControlCx" , SetGamePlayerControlCx );
	lua_register(lua, "SetGamePlayerControlCy" , SetGamePlayerControlCy );
	lua_register(lua, "SetGamePlayerControlCz" , SetGamePlayerControlCz );
	lua_register(lua, "SetGamePlayerControlBasespeed" , SetGamePlayerControlBasespeed );
	lua_register(lua, "SetGamePlayerControlCanRun" , SetGamePlayerControlCanRun );
	lua_register(lua, "SetGamePlayerControlMaxspeed" , SetGamePlayerControlMaxspeed );
	lua_register(lua, "SetGamePlayerControlTopspeed" , SetGamePlayerControlTopspeed );
	lua_register(lua, "SetGamePlayerControlMovement" , SetGamePlayerControlMovement );
	lua_register(lua, "SetGamePlayerControlMovey" , SetGamePlayerControlMovey );
	lua_register(lua, "SetGamePlayerControlLastMovement" , SetGamePlayerControlLastMovement );
	lua_register(lua, "SetGamePlayerControlFootfallCount" , SetGamePlayerControlFootfallCount );
	lua_register(lua, "SetGamePlayerControlLastMovement" , SetGamePlayerControlLastMovement );
	lua_register(lua, "SetGamePlayerControlGravityActive" , SetGamePlayerControlGravityActive );
	lua_register(lua, "SetGamePlayerControlPlrHitFloorMaterial" , SetGamePlayerControlPlrHitFloorMaterial );
	lua_register(lua, "SetGamePlayerControlUnderwater" , SetGamePlayerControlUnderwater );
	lua_register(lua, "SetGamePlayerControlJumpMode" , SetGamePlayerControlJumpMode );
	lua_register(lua, "SetGamePlayerControlJumpModeCanAffectVelocityCountdown" , SetGamePlayerControlJumpModeCanAffectVelocityCountdown );
	lua_register(lua, "SetGamePlayerControlSpeed" , SetGamePlayerControlSpeed );
	lua_register(lua, "SetGamePlayerControlAccel" , SetGamePlayerControlAccel );
	lua_register(lua, "SetGamePlayerControlSpeedRatio" , SetGamePlayerControlSpeedRatio );
	lua_register(lua, "SetGamePlayerControlWobble" , SetGamePlayerControlWobble );
	lua_register(lua, "SetGamePlayerControlWobbleSpeed" , SetGamePlayerControlWobbleSpeed );
	lua_register(lua, "SetGamePlayerControlWobbleHeight" , SetGamePlayerControlWobbleHeight );
	lua_register(lua, "SetGamePlayerControlJumpmax" , SetGamePlayerControlJumpmax );
	lua_register(lua, "SetGamePlayerControlPushangle" , SetGamePlayerControlPushangle );
	lua_register(lua, "SetGamePlayerControlPushforce" , SetGamePlayerControlPushforce );
	lua_register(lua, "SetGamePlayerControlFootfallPace" , SetGamePlayerControlFootfallPace );
	lua_register(lua, "SetGamePlayerControlFinalCameraAngley" , SetGamePlayerControlFinalCameraAngley );
	lua_register(lua, "SetGamePlayerControlLockAtHeight" , SetGamePlayerControlLockAtHeight );
	lua_register(lua, "SetGamePlayerControlControlHeight" , SetGamePlayerControlControlHeight );
	lua_register(lua, "SetGamePlayerControlControlHeightCooldown" , SetGamePlayerControlControlHeightCooldown );
	lua_register(lua, "SetGamePlayerControlStoreMovey" , SetGamePlayerControlStoreMovey );
	lua_register(lua, "SetGamePlayerControlPlrHitFloorMaterial" , SetGamePlayerControlPlrHitFloorMaterial );
	lua_register(lua, "SetGamePlayerControlHurtFall" , SetGamePlayerControlHurtFall );
	lua_register(lua, "SetGamePlayerControlLeanoverAngle" , SetGamePlayerControlLeanoverAngle );
	lua_register(lua, "SetGamePlayerControlLeanover" , SetGamePlayerControlLeanover );
	lua_register(lua, "SetGamePlayerControlCameraShake" , SetGamePlayerControlCameraShake );
	lua_register(lua, "SetGamePlayerControlFinalCameraAnglex" , SetGamePlayerControlFinalCameraAnglex );
	lua_register(lua, "SetGamePlayerControlFinalCameraAngley" , SetGamePlayerControlFinalCameraAngley );
	lua_register(lua, "SetGamePlayerControlFinalCameraAnglez" , SetGamePlayerControlFinalCameraAnglez );
	lua_register(lua, "SetGamePlayerControlCamRightMouseMode" , SetGamePlayerControlCamRightMouseMode );
	lua_register(lua, "SetGamePlayerControlCamCollisionSmooth" , SetGamePlayerControlCamCollisionSmooth );
	lua_register(lua, "SetGamePlayerControlCamCurrentDistance" , SetGamePlayerControlCamCurrentDistance );
	lua_register(lua, "SetGamePlayerControlCamDoFullRayCheck" , SetGamePlayerControlCamDoFullRayCheck );
	lua_register(lua, "SetGamePlayerControlLastGoodcx" , SetGamePlayerControlLastGoodcx );
	lua_register(lua, "SetGamePlayerControlLastGoodcy" , SetGamePlayerControlLastGoodcy );
	lua_register(lua, "SetGamePlayerControlLastGoodcz" , SetGamePlayerControlLastGoodcz );
	lua_register(lua, "SetGamePlayerControlCamDoFullRayCheck" , SetGamePlayerControlCamDoFullRayCheck );
	lua_register(lua, "SetGamePlayerControlFlinchx" , SetGamePlayerControlFlinchx );
	lua_register(lua, "SetGamePlayerControlFlinchy" , SetGamePlayerControlFlinchy );
	lua_register(lua, "SetGamePlayerControlFlinchz" , SetGamePlayerControlFlinchz );
	lua_register(lua, "SetGamePlayerControlFlinchCurrentx" , SetGamePlayerControlFlinchCurrentx );
	lua_register(lua, "SetGamePlayerControlFlinchCurrenty" , SetGamePlayerControlFlinchCurrenty );
	lua_register(lua, "SetGamePlayerControlFlinchCurrentz" , SetGamePlayerControlFlinchCurrentz );
	lua_register(lua, "SetGamePlayerControlFootfallType" , SetGamePlayerControlFootfallType );
	lua_register(lua, "SetGamePlayerControlRippleCount" , SetGamePlayerControlRippleCount );
	lua_register(lua, "SetGamePlayerControlLastFootfallSound" , SetGamePlayerControlLastFootfallSound );
	lua_register(lua, "SetGamePlayerControlInWaterState" , SetGamePlayerControlInWaterState );
	lua_register(lua, "SetGamePlayerControlDrownTimestamp" , SetGamePlayerControlDrownTimestamp );
	lua_register(lua, "SetGamePlayerControlDeadTime" , SetGamePlayerControlDeadTime );
	lua_register(lua, "SetGamePlayerControlSwimTimestamp" , SetGamePlayerControlSwimTimestamp );
	lua_register(lua, "SetGamePlayerControlRedDeathFog" , SetGamePlayerControlRedDeathFog );
	lua_register(lua, "SetGamePlayerControlHeartbeatTimeStamp" , SetGamePlayerControlHeartbeatTimeStamp );
	lua_register(lua, "SetGamePlayerControlThirdpersonEnabled" , SetGamePlayerControlThirdpersonEnabled );
	lua_register(lua, "SetGamePlayerControlThirdpersonCharacterIndex" , SetGamePlayerControlThirdpersonCharacterIndex );
	lua_register(lua, "SetGamePlayerControlThirdpersonCameraFollow" , SetGamePlayerControlThirdpersonCameraFollow );
	lua_register(lua, "SetGamePlayerControlThirdpersonCameraFocus" , SetGamePlayerControlThirdpersonCameraFocus );
	lua_register(lua, "SetGamePlayerControlThirdpersonCharactere" , SetGamePlayerControlThirdpersonCharactere );
	lua_register(lua, "SetGamePlayerControlThirdpersonCameraFollow" , SetGamePlayerControlThirdpersonCameraFollow );
	lua_register(lua, "SetGamePlayerControlThirdpersonShotFired" , SetGamePlayerControlThirdpersonShotFired );
	lua_register(lua, "SetGamePlayerControlThirdpersonCameraDistance" , SetGamePlayerControlThirdpersonCameraDistance );
	lua_register(lua, "SetGamePlayerControlThirdpersonCameraSpeed" , SetGamePlayerControlThirdpersonCameraSpeed );
	lua_register(lua, "SetGamePlayerControlThirdpersonCameraLocked" , SetGamePlayerControlThirdpersonCameraLocked );
	lua_register(lua, "SetGamePlayerControlThirdpersonCameraHeight" , SetGamePlayerControlThirdpersonCameraHeight );
	lua_register(lua, "SetGamePlayerControlThirdpersonCameraShoulder" , SetGamePlayerControlThirdpersonCameraShoulder );

	lua_register(lua, "SetGamePlayerStateGunMode" , SetGamePlayerStateGunMode );
	lua_register(lua, "GetGamePlayerStateGunMode" , GetGamePlayerStateGunMode );
	lua_register(lua, "SetGamePlayerStateFiringMode" , SetGamePlayerStateFiringMode );
	lua_register(lua, "GetGamePlayerStateFiringMode" , GetGamePlayerStateFiringMode );
	lua_register(lua, "GetGamePlayerStateWeaponAmmoIndex" , GetGamePlayerStateWeaponAmmoIndex );
	lua_register(lua, "GetGamePlayerStateAmmoOffset" , GetGamePlayerStateAmmoOffset );
	lua_register(lua, "GetGamePlayerStateGunMeleeKey" , GetGamePlayerStateGunMeleeKey );
	lua_register(lua, "SetGamePlayerStateBlockingAction", SetGamePlayerStateBlockingAction);
	lua_register(lua, "GetGamePlayerStateBlockingAction", GetGamePlayerStateBlockingAction);
	lua_register(lua, "SetGamePlayerStateGunShootNoAmmo" , SetGamePlayerStateGunShootNoAmmo );
	lua_register(lua, "GetGamePlayerStateGunShootNoAmmo" , GetGamePlayerStateGunShootNoAmmo );
	lua_register(lua, "SetGamePlayerStateUnderwater" , SetGamePlayerStateUnderwater );
	lua_register(lua, "GetGamePlayerStateUnderwater" , GetGamePlayerStateUnderwater );
	lua_register(lua, "SetGamePlayerStateRightMouseHold" , SetGamePlayerStateRightMouseHold );
	lua_register(lua, "GetGamePlayerStateRightMouseHold" , GetGamePlayerStateRightMouseHold );
	lua_register(lua, "SetGamePlayerStateXBOX" , SetGamePlayerStateXBOX );
	lua_register(lua, "GetGamePlayerStateXBOX" , GetGamePlayerStateXBOX );
	lua_register(lua, "SetGamePlayerStateXBOXControllerType" , SetGamePlayerStateXBOXControllerType );
	lua_register(lua, "GetGamePlayerStateXBOXControllerType" , GetGamePlayerStateXBOXControllerType );
	lua_register(lua, "JoystickX" , JoystickY );
	lua_register(lua, "JoystickY" , JoystickX );
	lua_register(lua, "JoystickZ" , JoystickZ );
	lua_register(lua, "SetGamePlayerStateGunZoomMode" , SetGamePlayerStateGunZoomMode );
	lua_register(lua, "GetGamePlayerStateGunZoomMode" , GetGamePlayerStateGunZoomMode );
	lua_register(lua, "SetGamePlayerStateGunZoomMag" , SetGamePlayerStateGunZoomMag );
	lua_register(lua, "GetGamePlayerStateGunZoomMag" , GetGamePlayerStateGunZoomMag );
	lua_register(lua, "SetGamePlayerStateGunReloadNoAmmo" , SetGamePlayerStateGunReloadNoAmmo );
	lua_register(lua, "GetGamePlayerStateGunReloadNoAmmo" , GetGamePlayerStateGunReloadNoAmmo );
	lua_register(lua, "SetGamePlayerStatePlrReloading" , SetGamePlayerStatePlrReloading );
	lua_register(lua, "GetGamePlayerStatePlrReloading" , GetGamePlayerStatePlrReloading );
	lua_register(lua, "SetGamePlayerStateGunAltSwapKey1" , SetGamePlayerStateGunAltSwapKey1 );
	lua_register(lua, "GetGamePlayerStateGunAltSwapKey1" , GetGamePlayerStateGunAltSwapKey1 );
	lua_register(lua, "SetGamePlayerStateGunAltSwapKey2" , SetGamePlayerStateGunAltSwapKey2 );
	lua_register(lua, "GetGamePlayerStateGunAltSwapKey2" , GetGamePlayerStateGunAltSwapKey2 );
	lua_register(lua, "SetGamePlayerStateWeaponKeySelection" , SetGamePlayerStateWeaponKeySelection );
	lua_register(lua, "GetGamePlayerStateWeaponKeySelection" , GetGamePlayerStateWeaponKeySelection );
	lua_register(lua, "SetGamePlayerStateWeaponIndex" , SetGamePlayerStateWeaponIndex );
	lua_register(lua, "GetGamePlayerStateWeaponIndex" , GetGamePlayerStateWeaponIndex );
	lua_register(lua, "SetGamePlayerStateCommandNewWeapon" , SetGamePlayerStateCommandNewWeapon );
	lua_register(lua, "GetGamePlayerStateCommandNewWeapon" , GetGamePlayerStateCommandNewWeapon );
	lua_register(lua, "SetGamePlayerStateGunID" , SetGamePlayerStateGunID );
	lua_register(lua, "GetGamePlayerStateGunID" , GetGamePlayerStateGunID );
	lua_register(lua, "SetGamePlayerStateGunSelectionAfterHide" , SetGamePlayerStateGunSelectionAfterHide );
	lua_register(lua, "GetGamePlayerStateGunSelectionAfterHide" , GetGamePlayerStateGunSelectionAfterHide );
	lua_register(lua, "SetGamePlayerStateGunBurst" , SetGamePlayerStateGunBurst );
	lua_register(lua, "GetGamePlayerStateGunBurst", GetGamePlayerStateGunBurst);
	lua_register(lua, "SetGamePlayerStatePlrKeyForceKeystate", SetGamePlayerStatePlrKeyForceKeystate);
	lua_register(lua, "JoystickHatAngle" , JoystickHatAngle );
	lua_register(lua, "JoystickFireXL" , JoystickFireXL );
	lua_register(lua, "JoystickTwistX" , JoystickTwistX );
	lua_register(lua, "JoystickTwistY" , JoystickTwistY );
	lua_register(lua, "JoystickTwistZ" , JoystickTwistZ );
	lua_register(lua, "SetGamePlayerStatePlrZoomInChange" , SetGamePlayerStatePlrZoomInChange );
	lua_register(lua, "GetGamePlayerStatePlrZoomInChange" , GetGamePlayerStatePlrZoomInChange );
	lua_register(lua, "SetGamePlayerStatePlrZoomIn" , SetGamePlayerStatePlrZoomIn );
	lua_register(lua, "GetGamePlayerStatePlrZoomIn" , GetGamePlayerStatePlrZoomIn );
	lua_register(lua, "SetGamePlayerStateLuaActiveMouse" , SetGamePlayerStateLuaActiveMouse );
	lua_register(lua, "GetGamePlayerStateLuaActiveMouse" , GetGamePlayerStateLuaActiveMouse );
	lua_register(lua, "SetGamePlayerStateRealFov" , SetGamePlayerStateRealFov );
	lua_register(lua, "GetGamePlayerStateRealFov" , GetGamePlayerStateRealFov );
	lua_register(lua, "SetGamePlayerStateDisablePeeking" , SetGamePlayerStateDisablePeeking );
	lua_register(lua, "GetGamePlayerStateDisablePeeking" , GetGamePlayerStateDisablePeeking );
	lua_register(lua, "SetGamePlayerStatePlrHasFocus" , SetGamePlayerStatePlrHasFocus );
	lua_register(lua, "GetGamePlayerStatePlrHasFocus" , GetGamePlayerStatePlrHasFocus );
	lua_register(lua, "SetGamePlayerStateGameRunAsMultiplayer" , SetGamePlayerStateGameRunAsMultiplayer );
	lua_register(lua, "GetGamePlayerStateGameRunAsMultiplayer" , GetGamePlayerStateGameRunAsMultiplayer );
	lua_register(lua, "SetGamePlayerStateSteamWorksRespawnLeft" , SetGamePlayerStateSteamWorksRespawnLeft );
	lua_register(lua, "GetGamePlayerStateSteamWorksRespawnLeft" , GetGamePlayerStateSteamWorksRespawnLeft );
	lua_register(lua, "SetGamePlayerStateMPRespawnLeft" , SetGamePlayerStateMPRespawnLeft );
	lua_register(lua, "GetGamePlayerStateMPRespawnLeft" , GetGamePlayerStateMPRespawnLeft );
	lua_register(lua, "SetGamePlayerStateTabMode" , SetGamePlayerStateTabMode );
	lua_register(lua, "GetGamePlayerStateTabMode" , GetGamePlayerStateTabMode );
	lua_register(lua, "SetGamePlayerStateLowFpsWarning" , SetGamePlayerStateLowFpsWarning );
	lua_register(lua, "GetGamePlayerStateLowFpsWarning" , GetGamePlayerStateLowFpsWarning );
	lua_register(lua, "SetGamePlayerStateCameraFov" , SetGamePlayerStateCameraFov );
	lua_register(lua, "GetGamePlayerStateCameraFov" , GetGamePlayerStateCameraFov );

	lua_register(lua, "GetPlayerFov", GetPlayerFov);
	lua_register(lua, "GetPlayerAttacking", GetPlayerAttacking);
	lua_register(lua, "PushPlayer", PushPlayer);
	
	lua_register(lua, "SetGamePlayerStateCameraFovZoomed" , SetGamePlayerStateCameraFovZoomed );
	lua_register(lua, "GetGamePlayerStateCameraFovZoomed" , GetGamePlayerStateCameraFovZoomed );
	lua_register(lua, "SetGamePlayerStateMouseInvert" , SetGamePlayerStateMouseInvert );
	lua_register(lua, "GetGamePlayerStateMouseInvert" , GetGamePlayerStateMouseInvert );

	lua_register(lua, "SetGamePlayerStateSlowMotion" , SetGamePlayerStateSlowMotion );
	lua_register(lua, "GetGamePlayerStateSlowMotion" , GetGamePlayerStateSlowMotion );
	lua_register(lua, "SetGamePlayerStateSmoothCameraKeys" , SetGamePlayerStateSmoothCameraKeys );
	lua_register(lua, "GetGamePlayerStateSmoothCameraKeys" , GetGamePlayerStateSmoothCameraKeys );
	lua_register(lua, "SetGamePlayerStateCamMouseMoveX" , SetGamePlayerStateCamMouseMoveX );
	lua_register(lua, "GetGamePlayerStateCamMouseMoveX" , GetGamePlayerStateCamMouseMoveX );
	lua_register(lua, "SetGamePlayerStateCamMouseMoveY" , SetGamePlayerStateCamMouseMoveY );
	lua_register(lua, "GetGamePlayerStateCamMouseMoveY" , GetGamePlayerStateCamMouseMoveY );
	lua_register(lua, "SetGamePlayerStateGunRecoilX" , SetGamePlayerStateGunRecoilX );
	lua_register(lua, "GetGamePlayerStateGunRecoilX" , GetGamePlayerStateGunRecoilX );
	lua_register(lua, "SetGamePlayerStateGunRecoilY" , SetGamePlayerStateGunRecoilY );
	lua_register(lua, "GetGamePlayerStateGunRecoilY" , GetGamePlayerStateGunRecoilY );
	lua_register(lua, "SetGamePlayerStateGunRecoilAngleX" , SetGamePlayerStateGunRecoilAngleX );
	lua_register(lua, "GetGamePlayerStateGunRecoilAngleX" , GetGamePlayerStateGunRecoilAngleX );
	lua_register(lua, "SetGamePlayerStateGunRecoilAngleY" , SetGamePlayerStateGunRecoilAngleY );
	lua_register(lua, "GetGamePlayerStateGunRecoilAngleY" , GetGamePlayerStateGunRecoilAngleY );
	lua_register(lua, "SetGamePlayerStateGunRecoilCorrectY" , SetGamePlayerStateGunRecoilCorrectY );
	lua_register(lua, "GetGamePlayerStateGunRecoilCorrectY" , GetGamePlayerStateGunRecoilCorrectY );
	lua_register(lua, "SetGamePlayerStateGunRecoilCorrectX" , SetGamePlayerStateGunRecoilCorrectX );
	lua_register(lua, "GetGamePlayerStateGunRecoilCorrectX" , GetGamePlayerStateGunRecoilCorrectX );
	lua_register(lua, "SetGamePlayerStateGunRecoilCorrectAngleY" , SetGamePlayerStateGunRecoilCorrectAngleY );
	lua_register(lua, "GetGamePlayerStateGunRecoilCorrectAngleY" , GetGamePlayerStateGunRecoilCorrectAngleY );
	lua_register(lua, "SetGamePlayerStateGunRecoilCorrectAngleX" , SetGamePlayerStateGunRecoilCorrectAngleX );
	lua_register(lua, "GetGamePlayerStateGunRecoilCorrectAngleX" , GetGamePlayerStateGunRecoilCorrectAngleX );
	lua_register(lua, "SetGamePlayerStateCamAngleX" , SetGamePlayerStateCamAngleX );
	lua_register(lua, "GetGamePlayerStateCamAngleX" , GetGamePlayerStateCamAngleX );
	lua_register(lua, "SetGamePlayerStateCamAngleY" , SetGamePlayerStateCamAngleY );
	lua_register(lua, "GetGamePlayerStateCamAngleY" , GetGamePlayerStateCamAngleY );
	lua_register(lua, "SetGamePlayerStatePlayerDucking" , SetGamePlayerStatePlayerDucking );
	lua_register(lua, "GetGamePlayerStatePlayerDucking" , GetGamePlayerStatePlayerDucking );
	lua_register(lua, "SetGamePlayerStateEditModeActive" , SetGamePlayerStateEditModeActive );
	lua_register(lua, "GetGamePlayerStateEditModeActive" , GetGamePlayerStateEditModeActive );
	lua_register(lua, "SetGamePlayerStatePlrKeyShift" , SetGamePlayerStatePlrKeyShift );
	lua_register(lua, "GetGamePlayerStatePlrKeyShift" , GetGamePlayerStatePlrKeyShift );
	lua_register(lua, "SetGamePlayerStatePlrKeyShift2" , SetGamePlayerStatePlrKeyShift2 );
	lua_register(lua, "GetGamePlayerStatePlrKeyShift2" , GetGamePlayerStatePlrKeyShift2 );
	lua_register(lua, "SetGamePlayerStatePlrKeyControl" , SetGamePlayerStatePlrKeyControl );
	lua_register(lua, "GetGamePlayerStatePlrKeyControl" , GetGamePlayerStatePlrKeyControl );
	lua_register(lua, "SetGamePlayerStateNoWater" , SetGamePlayerStateNoWater );
	lua_register(lua, "GetGamePlayerStateNoWater" , GetGamePlayerStateNoWater );
	lua_register(lua, "SetGamePlayerStateWaterlineY" , SetGamePlayerStateWaterlineY );
	lua_register(lua, "GetGamePlayerStateWaterlineY" , GetGamePlayerStateWaterlineY );

	lua_register(lua, "SetGamePlayerStateFlashlightKeyEnabled" , SetGamePlayerStateFlashlightKeyEnabled );
	lua_register(lua, "GetGamePlayerStateFlashlightKeyEnabled" , GetGamePlayerStateFlashlightKeyEnabled );
	lua_register(lua, "SetGamePlayerStateFlashlightControl", SetGamePlayerStateFlashlightControl);
	lua_register(lua, "GetGamePlayerStateFlashlightControl", GetGamePlayerStateFlashlightControl);
	lua_register(lua, "SetGamePlayerStateFlashlightRange", SetGamePlayerStateFlashlightRange);
	lua_register(lua, "GetGamePlayerStateFlashlightRange", GetGamePlayerStateFlashlightRange);
	lua_register(lua, "SetGamePlayerStateFlashlightRadius", SetGamePlayerStateFlashlightRadius);
	lua_register(lua, "GetGamePlayerStateFlashlightRadius", GetGamePlayerStateFlashlightRadius);
	lua_register(lua, "SetGamePlayerStateFlashlightColorR", SetGamePlayerStateFlashlightColorR);
	lua_register(lua, "GetGamePlayerStateFlashlightColorR", GetGamePlayerStateFlashlightColorR);
	lua_register(lua, "SetGamePlayerStateFlashlightColorG", SetGamePlayerStateFlashlightColorG);
	lua_register(lua, "GetGamePlayerStateFlashlightColorG", GetGamePlayerStateFlashlightColorG);
	lua_register(lua, "SetGamePlayerStateFlashlightColorB", SetGamePlayerStateFlashlightColorB);
	lua_register(lua, "GetGamePlayerStateFlashlightColorB", GetGamePlayerStateFlashlightColorB);
	lua_register(lua, "SetGamePlayerStateFlashlightCastShadow", SetGamePlayerStateFlashlightCastShadow);
	lua_register(lua, "GetGamePlayerStateFlashlightCastShadow", GetGamePlayerStateFlashlightCastShadow);

	lua_register(lua, "SetGamePlayerStateMoving" , SetGamePlayerStateMoving );
	lua_register(lua, "GetGamePlayerStateMoving" , GetGamePlayerStateMoving );
	lua_register(lua, "SetGamePlayerStateTerrainHeight" , SetGamePlayerStateTerrainHeight );
	lua_register(lua, "GetGamePlayerStateTerrainHeight" , GetGamePlayerStateTerrainHeight );
	lua_register(lua, "SetGamePlayerStateJetpackVerticalMove" , SetGamePlayerStateJetpackVerticalMove );
	lua_register(lua, "GetGamePlayerStateJetpackVerticalMove" , GetGamePlayerStateJetpackVerticalMove );
	lua_register(lua, "SetGamePlayerStateTerrainID" , SetGamePlayerStateTerrainID );
	lua_register(lua, "GetGamePlayerStateTerrainID" , GetGamePlayerStateTerrainID );
	lua_register(lua, "SetGamePlayerStateEnablePlrSpeedMods" , SetGamePlayerStateEnablePlrSpeedMods );
	lua_register(lua, "GetGamePlayerStateEnablePlrSpeedMods" , GetGamePlayerStateEnablePlrSpeedMods );
	lua_register(lua, "SetGamePlayerStateRiftMode" , SetGamePlayerStateRiftMode );
	lua_register(lua, "GetGamePlayerStateRiftMode" , GetGamePlayerStateRiftMode );
	lua_register(lua, "SetGamePlayerStatePlayerX" , SetGamePlayerStatePlayerX );
	lua_register(lua, "GetGamePlayerStatePlayerX" , GetGamePlayerStatePlayerX );
	lua_register(lua, "SetGamePlayerStatePlayerY" , SetGamePlayerStatePlayerY );
	lua_register(lua, "GetGamePlayerStatePlayerY" , GetGamePlayerStatePlayerY );
	lua_register(lua, "SetGamePlayerStatePlayerZ" , SetGamePlayerStatePlayerZ );
	lua_register(lua, "GetGamePlayerStatePlayerZ" , GetGamePlayerStatePlayerZ );
	lua_register(lua, "SetGamePlayerStateTerrainPlayerX" , SetGamePlayerStateTerrainPlayerX );
	lua_register(lua, "GetGamePlayerStateTerrainPlayerX" , GetGamePlayerStateTerrainPlayerX );
	lua_register(lua, "SetGamePlayerStateTerrainPlayerY" , SetGamePlayerStateTerrainPlayerY );
	lua_register(lua, "GetGamePlayerStateTerrainPlayerY" , GetGamePlayerStateTerrainPlayerY );
	lua_register(lua, "SetGamePlayerStateTerrainPlayerZ" , SetGamePlayerStateTerrainPlayerZ );
	lua_register(lua, "GetGamePlayerStateTerrainPlayerZ" , GetGamePlayerStateTerrainPlayerZ );
	lua_register(lua, "SetGamePlayerStateTerrainPlayerAX" , SetGamePlayerStateTerrainPlayerAX );
	lua_register(lua, "GetGamePlayerStateTerrainPlayerAX" , GetGamePlayerStateTerrainPlayerAX );
	lua_register(lua, "SetGamePlayerStateTerrainPlayerAY" , SetGamePlayerStateTerrainPlayerAY );
	lua_register(lua, "GetGamePlayerStateTerrainPlayerAY" , GetGamePlayerStateTerrainPlayerAY );
	lua_register(lua, "SetGamePlayerStateTerrainPlayerAZ" , SetGamePlayerStateTerrainPlayerAZ );
	lua_register(lua, "GetGamePlayerStateTerrainPlayerAZ" , GetGamePlayerStateTerrainPlayerAZ );
	lua_register(lua, "SetGamePlayerStateAdjustBasedOnWobbleY" , SetGamePlayerStateAdjustBasedOnWobbleY );
	lua_register(lua, "GetGamePlayerStateAdjustBasedOnWobbleY" , GetGamePlayerStateAdjustBasedOnWobbleY );
	lua_register(lua, "SetGamePlayerStateFinalCamX" , SetGamePlayerStateFinalCamX );
	lua_register(lua, "GetGamePlayerStateFinalCamX" , GetGamePlayerStateFinalCamX );
	lua_register(lua, "SetGamePlayerStateFinalCamY" , SetGamePlayerStateFinalCamY );
	lua_register(lua, "GetGamePlayerStateFinalCamY" , GetGamePlayerStateFinalCamY );
	lua_register(lua, "SetGamePlayerStateFinalCamZ" , SetGamePlayerStateFinalCamZ );
	lua_register(lua, "GetGamePlayerStateFinalCamZ" , GetGamePlayerStateFinalCamZ );
	lua_register(lua, "SetGamePlayerStateShakeX" , SetGamePlayerStateShakeX );
	lua_register(lua, "GetGamePlayerStateShakeX" , GetGamePlayerStateShakeX );
	lua_register(lua, "SetGamePlayerStateShakeY" , SetGamePlayerStateShakeY );
	lua_register(lua, "GetGamePlayerStateShakeY" , GetGamePlayerStateShakeY );
	lua_register(lua, "SetGamePlayerStateShakeZ" , SetGamePlayerStateShakeZ );
	lua_register(lua, "GetGamePlayerStateShakeZ" , GetGamePlayerStateShakeZ );
	lua_register(lua, "SetGamePlayerStateImmunity" , SetGamePlayerStateImmunity );
	lua_register(lua, "GetGamePlayerStateImmunity" , GetGamePlayerStateImmunity );
	lua_register(lua, "SetGamePlayerStateCharAnimIndex" , SetGamePlayerStateCharAnimIndex );
	lua_register(lua, "GetGamePlayerStateCharAnimIndex" , GetGamePlayerStateCharAnimIndex );

	lua_register(lua, "SetGamePlayerStateCounteredAction", SetGamePlayerStateCounteredAction);
	lua_register(lua, "GetGamePlayerStateCounteredAction", GetGamePlayerStateCounteredAction);

	lua_register(lua, "GetCharacterForwardX", GetCharacterForwardX);
	lua_register(lua, "GetCharacterForwardY", GetCharacterForwardY);
	lua_register(lua, "GetCharacterForwardZ", GetCharacterForwardZ);

	lua_register(lua, "GetGamePlayerStateMotionController" , GetGamePlayerStateMotionController );
	lua_register(lua, "GetGamePlayerStateMotionControllerType" , GetGamePlayerStateMotionControllerType );
	lua_register(lua, "MotionControllerThumbstickX" , MotionControllerThumbstickX );
	lua_register(lua, "MotionControllerThumbstickY" , MotionControllerThumbstickY );
	lua_register(lua, "CombatControllerTrigger" , CombatControllerTrigger );
	lua_register(lua, "CombatControllerGrip", CombatControllerGrip);
	lua_register(lua, "CombatControllerButtonA", CombatControllerButtonA);
	lua_register(lua, "CombatControllerButtonB", CombatControllerButtonB);
	lua_register(lua, "CombatControllerThumbstickX" , CombatControllerThumbstickX );
	lua_register(lua, "CombatControllerThumbstickY" , CombatControllerThumbstickY );
	lua_register(lua, "MotionControllerBestX" , MotionControllerBestX );
	lua_register(lua, "MotionControllerBestY" , MotionControllerBestY );
	lua_register(lua, "MotionControllerBestZ" , MotionControllerBestZ );
	lua_register(lua, "MotionControllerBestAngleX" , MotionControllerBestAngleX );
	lua_register(lua, "MotionControllerBestAngleY" , MotionControllerBestAngleY );
	lua_register(lua, "MotionControllerBestAngleZ" , MotionControllerBestAngleZ );
	lua_register(lua, "MotionControllerLaserGuidedEntityObj", MotionControllerLaserGuidedEntityObj);
	lua_register(lua, "CombatControllerLaserGuidedHit", CombatControllerLaserGuidedHit);

	lua_register(lua, "SetGamePlayerStateIsMelee" , SetGamePlayerStateIsMelee );
	lua_register(lua, "GetGamePlayerStateIsMelee" , GetGamePlayerStateIsMelee );
	lua_register(lua, "SetGamePlayerStateAlternate" , SetGamePlayerStateAlternate );
	lua_register(lua, "GetGamePlayerStateAlternate" , GetGamePlayerStateAlternate );
	lua_register(lua, "SetGamePlayerStateModeShareMags" , SetGamePlayerStateModeShareMags );
	lua_register(lua, "GetGamePlayerStateModeShareMags" , GetGamePlayerStateModeShareMags );
	lua_register(lua, "SetGamePlayerStateAlternateIsFlak" , SetGamePlayerStateAlternateIsFlak );
	lua_register(lua, "GetGamePlayerStateAlternateIsFlak" , GetGamePlayerStateAlternateIsFlak );
	lua_register(lua, "SetGamePlayerStateAlternateIsRay" , SetGamePlayerStateAlternateIsRay );
	lua_register(lua, "GetGamePlayerStateAlternateIsRay" , GetGamePlayerStateAlternateIsRay );
	lua_register(lua, "SetFireModeSettingsReloadQty" , SetFireModeSettingsReloadQty );
	lua_register(lua, "GetFireModeSettingsReloadQty" , GetFireModeSettingsReloadQty );
	lua_register(lua, "SetFireModeSettingsIsEmpty" , SetFireModeSettingsIsEmpty );
	lua_register(lua, "GetFireModeSettingsIsEmpty" , GetFireModeSettingsIsEmpty );
	lua_register(lua, "SetFireModeSettingsJammed" , SetFireModeSettingsJammed );
	lua_register(lua, "GetFireModeSettingsJammed" , GetFireModeSettingsJammed );
	lua_register(lua, "SetFireModeSettingsJamChance" , SetFireModeSettingsJamChance );
	lua_register(lua, "GetFireModeSettingsJamChance" , GetFireModeSettingsJamChance );
	lua_register(lua, "SetFireModeSettingsMinTimer" , SetFireModeSettingsMinTimer );
	lua_register(lua, "GetFireModeSettingsMinTimer" , GetFireModeSettingsMinTimer );
	lua_register(lua, "SetFireModeSettingsAddTimer" , SetFireModeSettingsAddTimer );
	lua_register(lua, "GetFireModeSettingsAddTimer" , GetFireModeSettingsAddTimer );
	lua_register(lua, "SetFireModeSettingsShotsFired" , SetFireModeSettingsShotsFired );
	lua_register(lua, "GetFireModeSettingsShotsFired" , GetFireModeSettingsShotsFired );
	lua_register(lua, "SetFireModeSettingsCoolTimer" , SetFireModeSettingsCoolTimer );
	lua_register(lua, "GetFireModeSettingsCoolTimer" , GetFireModeSettingsCoolTimer );
	lua_register(lua, "SetFireModeSettingsOverheatAfter" , SetFireModeSettingsOverheatAfter );
	lua_register(lua, "GetFireModeSettingsOverheatAfter" , GetFireModeSettingsOverheatAfter );
	lua_register(lua, "SetFireModeSettingsJamChanceTime" , SetFireModeSettingsJamChanceTime );
	lua_register(lua, "GetFireModeSettingsJamChanceTime" , GetFireModeSettingsJamChanceTime );
	lua_register(lua, "SetFireModeSettingsCoolDown" , SetFireModeSettingsCoolDown );
	lua_register(lua, "GetFireModeSettingsCoolDown" , GetFireModeSettingsCoolDown );
	lua_register(lua, "SetFireModeSettingsNoSubmergedFire" , SetFireModeSettingsNoSubmergedFire );
	lua_register(lua, "GetFireModeSettingsNoSubmergedFire" , GetFireModeSettingsNoSubmergedFire );
	lua_register(lua, "SetFireModeSettingsSimpleZoom" , SetFireModeSettingsSimpleZoom );
	lua_register(lua, "GetFireModeSettingsSimpleZoom" , GetFireModeSettingsSimpleZoom );
	lua_register(lua, "SetFireModeSettingsForceZoomOut" , SetFireModeSettingsForceZoomOut );
	lua_register(lua, "GetFireModeSettingsForceZoomOut" , GetFireModeSettingsForceZoomOut );
	lua_register(lua, "SetFireModeSettingsZoomMode" , SetFireModeSettingsZoomMode );
	lua_register(lua, "GetFireModeSettingsZoomMode" , GetFireModeSettingsZoomMode );
	lua_register(lua, "SetFireModeSettingsSimpleZoomAnim" , SetFireModeSettingsSimpleZoomAnim );
	lua_register(lua, "GetFireModeSettingsSimpleZoomAnim" , GetFireModeSettingsSimpleZoomAnim );
	lua_register(lua, "SetFireModeSettingsPoolIndex" , SetFireModeSettingsPoolIndex );
	lua_register(lua, "GetFireModeSettingsPoolIndex" , GetFireModeSettingsPoolIndex );
	lua_register(lua, "SetFireModeSettingsPlrTurnSpeedMod" , SetFireModeSettingsPlrTurnSpeedMod );
	lua_register(lua, "GetFireModeSettingsPlrTurnSpeedMod" , GetFireModeSettingsPlrTurnSpeedMod );
	lua_register(lua, "SetFireModeSettingsZoomTurnSpeed" , SetFireModeSettingsZoomTurnSpeed );
	lua_register(lua, "GetFireModeSettingsZoomTurnSpeed" , GetFireModeSettingsZoomTurnSpeed );
	lua_register(lua, "SetFireModeSettingsPlrJumpSpeedMod" , SetFireModeSettingsPlrJumpSpeedMod );
	lua_register(lua, "GetFireModeSettingsPlrJumpSpeedMod" , GetFireModeSettingsPlrJumpSpeedMod );
	lua_register(lua, "SetFireModeSettingsPlrEmptySpeedMod" , SetFireModeSettingsPlrEmptySpeedMod );
	lua_register(lua, "GetFireModeSettingsPlrEmptySpeedMod" , GetFireModeSettingsPlrEmptySpeedMod );
	lua_register(lua, "SetFireModeSettingsPlrMoveSpeedMod" , SetFireModeSettingsPlrMoveSpeedMod );
	lua_register(lua, "GetFireModeSettingsPlrMoveSpeedMod" , GetFireModeSettingsPlrMoveSpeedMod );
	lua_register(lua, "SetFireModeSettingsZoomWalkSpeed" , SetFireModeSettingsZoomWalkSpeed );
	lua_register(lua, "GetFireModeSettingsZoomWalkSpeed" , GetFireModeSettingsZoomWalkSpeed );
	lua_register(lua, "SetFireModeSettingsPlrReloadSpeedMod" , SetFireModeSettingsPlrReloadSpeedMod );
	lua_register(lua, "GetFireModeSettingsPlrReloadSpeedMod" , GetFireModeSettingsPlrReloadSpeedMod );
	lua_register(lua, "SetFireModeSettingsHasEmpty" , SetFireModeSettingsHasEmpty );
	lua_register(lua, "GetFireModeSettingsHasEmpty" , GetFireModeSettingsHasEmpty );
	lua_register(lua, "GetFireModeSettingsActionBlockStart", GetFireModeSettingsActionBlockStart);
	lua_register(lua, "GetFireModeSettingsMeleeWithRightClick", GetFireModeSettingsMeleeWithRightClick);
	lua_register(lua, "GetFireModeSettingsBlockWithRightClick", GetFireModeSettingsBlockWithRightClick);
		
	lua_register(lua, "SetGamePlayerStateGunSound" , SetGamePlayerStateGunSound );
	lua_register(lua, "GetGamePlayerStateGunSound" , GetGamePlayerStateGunSound );
	lua_register(lua, "SetGamePlayerStateGunAltSound" , SetGamePlayerStateGunAltSound );
	lua_register(lua, "GetGamePlayerStateGunAltSound" , GetGamePlayerStateGunAltSound );

	lua_register(lua, "CopyCharAnimState" , CopyCharAnimState );
	lua_register(lua, "SetCharAnimStatePlayCsi" , SetCharAnimStatePlayCsi );
	lua_register(lua, "GetCharAnimStatePlayCsi" , GetCharAnimStatePlayCsi );
	lua_register(lua, "SetCharAnimStateOriginalE" , SetCharAnimStateOriginalE );
	lua_register(lua, "GetCharAnimStateOriginalE" , GetCharAnimStateOriginalE );
	lua_register(lua, "SetCharAnimStateObj" , SetCharAnimStateObj );
	lua_register(lua, "GetCharAnimStateObj" , GetCharAnimStateObj );
	lua_register(lua, "SetCharAnimStateAnimationSpeed" , SetCharAnimStateAnimationSpeed );
	lua_register(lua, "GetCharAnimStateAnimationSpeed" , GetCharAnimStateAnimationSpeed );
	lua_register(lua, "SetCharAnimStateE" , SetCharAnimStateE );
	lua_register(lua, "GetCharAnimStateE" , GetCharAnimStateE );

	lua_register(lua, "GetCsiStoodVault" , GetCsiStoodVault );
	lua_register(lua, "GetCharSeqTrigger" , GetCharSeqTrigger );
	lua_register(lua, "GetEntityElementBankIndex" , GetEntityElementBankIndex );
	lua_register(lua, "GetEntityElementObj" , GetEntityElementObj );
	lua_register(lua, "GetEntityElementRagdollified" , GetEntityElementRagdollified );
	lua_register(lua, "GetEntityElementSpeedModulator" , GetEntityElementSpeedModulator );
	lua_register(lua, "GetEntityProfileJumpModifier" , GetEntityProfileJumpModifier );
	lua_register(lua, "GetEntityProfileStartOfAIAnim" , GetEntityProfileStartOfAIAnim );
	lua_register(lua, "GetEntityProfileJumpHold" , GetEntityProfileJumpHold );
	lua_register(lua, "GetEntityProfileJumpResume" , GetEntityProfileJumpResume );

	lua_register(lua, "SetCharAnimControlsLeaping" , SetCharAnimControlsLeaping );
	lua_register(lua, "GetCharAnimControlsLeaping" , GetCharAnimControlsLeaping );
	lua_register(lua, "SetCharAnimControlsMoving" , SetCharAnimControlsMoving );
	lua_register(lua, "GetCharAnimControlsMoving" , GetCharAnimControlsMoving );
	
	lua_register(lua, "GetEntityAnimStart" , GetEntityAnimStart );
	lua_register(lua, "GetEntityAnimFinish" , GetEntityAnimFinish );

	lua_register(lua, "SetRotationYSlowly" , SetRotationYSlowly );

	// OLD Particle System
	lua_register(lua, "ParticlesGetFreeEmitter" , ParticlesGetFreeEmitter );
	lua_register(lua, "ParticlesAddEmitter" ,     ParticlesAddEmitter );
	lua_register(lua, "ParticlesAddEmitterEx" ,   ParticlesAddEmitterEx );
	lua_register(lua, "ParticlesDeleteEmitter" ,  ParticlesDeleteEmitter );
	lua_register(lua, "ParticlesSpawnParticle",   ParticlesSpawnParticle );
	lua_register(lua, "ParticlesLoadImage",       ParticlesLoadImage );
	lua_register(lua, "ParticlesLoadEffect",      ParticlesLoadEffect );
	lua_register(lua, "ParticlesSetFrames",       ParticlesSetFrames );
	lua_register(lua, "ParticlesSetSpeed",        ParticlesSetSpeed );
	lua_register(lua, "ParticlesSetGravity",      ParticlesSetGravity );
	lua_register(lua, "ParticlesSetOffset",       ParticlesSetOffset );
	lua_register(lua, "ParticlesSetAngle",        ParticlesSetAngle );
	lua_register(lua, "ParticlesSetRotation",     ParticlesSetRotation );
	lua_register(lua, "ParticlesSetScale",        ParticlesSetScale );
	lua_register(lua, "ParticlesSetAlpha",        ParticlesSetAlpha );
	lua_register(lua, "ParticlesSetLife",         ParticlesSetLife );
	lua_register(lua, "ParticlesSetWindVector",   ParticlesSetWindVector );
	lua_register(lua, "ParticlesSetNoWind",       ParticlesSetNoWind );

	// NEW Particle Effects System
	lua_register(lua, "EffectStart",				EffectStart);
	lua_register(lua, "EffectStop",					EffectStop);
	lua_register(lua, "EffectSetLocalPosition",		EffectSetLocalPosition);
	lua_register(lua, "EffectSetLocalRotation",		EffectSetLocalRotation);
	lua_register(lua, "EffectSetSpeed",				EffectSetSpeed);
	lua_register(lua, "EffectSetOpacity",			EffectSetOpacity);
	lua_register(lua, "EffectGetOpacity",			EffectGetOpacity);
	lua_register(lua, "EffectSetParticleSize",		EffectSetParticleSize);
	lua_register(lua, "EffectSetBurstMode",			EffectSetBurstMode);
	lua_register(lua, "EffectFireBurst",			EffectFireBurst);
	lua_register(lua, "EffectSetFloorReflection",	EffectSetFloorReflection);
	lua_register(lua, "EffectSetBounciness",		EffectSetBounciness);
	lua_register(lua, "EffectSetColor",				EffectSetColor);
	lua_register(lua, "EffectSetLifespan",			EffectSetLifespan);

#ifdef WICKEDPARTICLESYSTEM
	//PE: Wicked particle system.
	lua_register(lua, "WParticleEffectLoad", WParticleEffectLoad);
	lua_register(lua, "WParticleEffectPosition", WParticleEffectPosition);
	lua_register(lua, "WParticleEffectVisible", WParticleEffectVisible);
	lua_register(lua, "WParticleEffectAction", WParticleEffectAction);
	lua_register(lua, "WParticleEffectSetKillBox", WParticleEffectSetKillBox);
	lua_register(lua, "WParticleEffectClearKillBoxes", WParticleEffectClearKillBoxes);
	lua_register(lua, "WParticleEffectSetOpacity", WParticleEffectSetOpacity);
	lua_register(lua, "WParticleEffectGetOpacity", WParticleEffectGetOpacity);
	lua_register(lua, "WParticleEffectSetSize", WParticleEffectSetSize);
	lua_register(lua, "WParticleEffectGetSize", WParticleEffectGetSize);
	lua_register(lua, "WParticleEffectSetColor", WParticleEffectSetColor);
	lua_register(lua, "WParticleEffectGetColor", WParticleEffectGetColor);
	lua_register(lua, "WParticleEffectGetBounds", WParticleEffectGetBounds);
#endif

	//PE: Other missing commands.
	lua_register(lua, "PositionSound", entity_lua_positionsound);
	lua_register(lua, "AddTracer", AddTracer);
	lua_register(lua, "LoadTracerImage", LoadTracerImage);


	lua_register(lua, "GetBulletHit",             GetBulletHit);
	lua_register(lua, "SetFlashLight" , SetFlashLight );
	lua_register(lua, "SetFlashLightPosition", SetFlashLightPosition);
	lua_register(lua, "GetFlashLightPosition", GetFlashLightPosition);

	lua_register(lua, "SetAttachmentVisible" , SetAttachmentVisible );
	lua_register(lua, "SetEntityInstanceEmissive" , SetEntityInstanceEmissive );
	lua_register(lua, "AddBulletHole" , AddBulletHole );
	lua_register(lua, "BulletRay" , BulletRay );
	lua_register(lua, "GetPlayerHitSeq" , GetPlayerHitSeq );
	lua_register(lua, "GetPlayerHit" , GetPlayerHit );
	lua_register(lua, "GetEntityInstanceEmissive" , GetEntityInstanceEmissive );
	lua_register(lua, "SetEntityInstanceTint" , SetEntityInstanceTint );
	lua_register(lua, "GetEntityInstanceTint" , GetEntityInstanceTint );
	lua_register(lua, "GetEntityImmunity" , GetEntityImmunity );
	lua_register(lua, "SetOcclusion" , SetOcclusion );
	lua_register(lua, "SetPlayerWeapons", SetPlayerWeapons);
	lua_register(lua, "FirePlayerWeapon", FirePlayerWeapon);
	lua_register(lua, "SetFlashLightKeyEnabled" , SetFlashLightKeyEnabled );
	lua_register(lua, "SetPlayerRun" , SetPlayerRun );
	lua_register(lua, "SetFont" , SetFont );
	lua_register(lua, "GetTextWidth" , GetTextWidth );
	lua_register(lua, "GetTextHeight" , GetTextHeight );
	lua_register(lua, "GetDeviceWidth" , GetDeviceWidth );
	lua_register(lua, "GetDeviceHeight" , GetDeviceHeight );
	lua_register(lua, "GetFirstEntitySpawn" , GetFirstEntitySpawn );
	lua_register(lua, "GetNextEntitySpawn" , GetFirstEntitySpawn );	

	// VR and head tracking
	lua_register(lua, "GetHeadTracker" , GetHeadTracker );	
	lua_register(lua, "ResetHeadTracker" , ResetHeadTracker );	
	lua_register(lua, "GetHeadTrackerYaw" , GetHeadTrackerYaw );	
	lua_register(lua, "GetHeadTrackerPitch" , GetHeadTrackerPitch );	
	lua_register(lua, "GetHeadTrackerRoll" , GetHeadTrackerRoll );	
	lua_register(lua, "GetHeadTrackerNormalX" , GetHeadTrackerNormalX );	
	lua_register(lua, "GetHeadTrackerNormalY" , GetHeadTrackerNormalY );	
	lua_register(lua, "GetHeadTrackerNormalZ" , GetHeadTrackerNormalZ );		

	// Prompt3D
	lua_register(lua, "Prompt3D" , Prompt3D );	
	lua_register(lua, "PositionPrompt3D" , PositionPrompt3D );	
	lua_register(lua, "PromptLocalDuration", PromptLocalDuration);

	// utility
	lua_register(lua, "MsgBox" , MsgBox );
	lua_register(lua, "IsTestGame", GetIsTestgame);

	//Cloud Shader
	lua_register(lua, "GetCloudDensity", GetCloudDensity);
	lua_register(lua, "GetCloudCoverage", GetCloudCoverage);
	lua_register(lua, "GetCloudHeight", GetCloudHeight);
	lua_register(lua, "GetCloudThickness", GetCloudThickness);
	lua_register(lua, "GetCloudSpeed", GetCloudSpeed);
	lua_register(lua, "SetCloudDensity", SetCloudDensity);
	lua_register(lua, "SetCloudCoverage", SetCloudCoverage);
	lua_register(lua, "SetCloudHeight", SetCloudHeight);
	lua_register(lua, "SetCloudThickness", SetCloudThickness);
	lua_register(lua, "SetCloudSpeed", SetCloudSpeed);

	//PE: Other Shader
	lua_register(lua, "SetTreeWind", SetTreeWind);
	lua_register(lua, "GetTreeWind", GetTreeWind);
	lua_register(lua, "SetTreeSubsurface", SetTreeSubsurface);
	lua_register(lua, "GetTreeSubsurface", GetTreeSubsurface);

	//Water Shader
	//setter
	lua_register(lua, "SetWaterHeight", SetWaterHeight);
	lua_register(lua, "SetWaterColor", SetWaterShaderColor);
	lua_register(lua, "SetWaterWaveIntensity", SetWaterWaveIntensity);
	lua_register(lua, "SetWaterTransparancy", SetWaterTransparancy);
	lua_register(lua, "SetWaterFog", SetWaterFog);
	lua_register(lua, "GetWaterFog", GetWaterFog);
	lua_register(lua, "SetWaterReflection", SetWaterReflection);
	lua_register(lua, "SetWaterReflectionSparkleIntensity", SetWaterReflectionSparkleIntensity);
	lua_register(lua, "SetWaterFlowDirection", SetWaterFlowDirection);
	lua_register(lua, "SetWaterDistortionWaves", SetWaterDistortionWaves);
	lua_register(lua, "SetRippleWaterSpeed", SetRippleWaterSpeed);
	//getter
	lua_register(lua, "GetWaterHeight", GetWaterHeight);
	lua_register(lua, "GetWaterWaveIntensity", GetWaterWaveIntensity);
	lua_register(lua, "GetWaterShaderColorRed", GetWaterShaderColorRed);
	lua_register(lua, "GetWaterShaderColorGreen", GetWaterShaderColorGreen);
	lua_register(lua, "GetWaterShaderColorBlue", GetWaterShaderColorBlue);
	lua_register(lua, "GetWaterTransparancy", GetWaterTransparancy);
	lua_register(lua, "GetWaterReflection", GetWaterReflection);
	lua_register(lua, "GetWaterReflectionSparkleIntensity", GetWaterReflectionSparkleIntensity);
	lua_register(lua, "GetWaterFlowDirectionX", GetWaterFlowDirectionX);
	lua_register(lua, "GetWaterFlowDirectionY", GetWaterFlowDirectionY);
	lua_register(lua, "GetWaterFlowSpeed", GetWaterFlowSpeed);
	lua_register(lua, "GetWaterDistortionWaves", GetWaterDistortionWaves);
	lua_register(lua, "GetRippleWaterSpeed", GetRippleWaterSpeed); 
	lua_register(lua, "GetWaterEnabled", GetWaterEnabled);

	#ifdef STORYBOARD
	//Storyboard
	lua_register(lua, "SetScreenHUDGlobalScale", SetScreenHUDGlobalScale);
	lua_register(lua, "InitScreen", InitScreen);
	lua_register(lua, "DisplayScreen", DisplayScreen);
	lua_register(lua, "ForceMouseXYClick", ForceMouseXYClick);
	lua_register(lua, "DisplayCurrentScreen", DisplayCurrentScreen);
	lua_register(lua, "GetCurrentScreen", GetCurrentScreen);
	lua_register(lua, "GetCurrentScreenName", GetCurrentScreenName);
	lua_register(lua, "CheckScreenToggles", CheckScreenToggles);
	lua_register(lua, "DisableGunFireInHUD", DisableGunFireInHUD);
	lua_register(lua, "EnableGunFireInHUD", EnableGunFireInHUD);
	lua_register(lua, "DisableBoundHudKeys", DisableBoundHudKeys);
	lua_register(lua, "EnableBoundHudKeys", EnableBoundHudKeys);
	
	lua_register(lua, "ScreenToggle", ScreenToggle);
	lua_register(lua, "ScreenToggleByKey", ScreenToggleByKey);
	lua_register(lua, "GetIfUsingTABScreen", GetIfUsingTABScreen);
	lua_register(lua, "GetStoryboardActive", GetStoryboardActive);
	lua_register(lua, "GetScreenWidgetValue", GetScreenWidgetValue);
	lua_register(lua, "SetScreenWidgetValue", SetScreenWidgetValue);
	lua_register(lua, "SetScreenWidgetSelection", SetScreenWidgetSelection);
	lua_register(lua, "GetScreenElementsType", GetScreenElementsType);
	lua_register(lua, "GetScreenElementTypeID", GetScreenElementTypeID);
	lua_register(lua, "GetScreenElements", GetScreenElements);
	lua_register(lua, "GetScreenElementID", GetScreenElementID);
	lua_register(lua, "GetScreenElementImage", GetScreenElementImage);
	lua_register(lua, "GetScreenElementArea", GetScreenElementArea);
	lua_register(lua, "GetScreenElementDetails", GetScreenElementDetails);
	lua_register(lua, "GetScreenElementName", GetScreenElementName);
	lua_register(lua, "SetScreenElementVisibility", SetScreenElementVisibility);
	lua_register(lua, "SetScreenElementPosition", SetScreenElementPosition);
	lua_register(lua, "SetScreenElementText", SetScreenElementText);
	lua_register(lua, "SetScreenElementColor", SetScreenElementColor);
	lua_register(lua, "GetCollectionAttributeQuantity", GetCollectionAttributeQuantity);
	lua_register(lua, "GetCollectionAttributeLabel", GetCollectionAttributeLabel);
	lua_register(lua, "GetCollectionItemQuantity", GetCollectionItemQuantity);
	lua_register(lua, "GetCollectionItemAttribute", GetCollectionItemAttribute);	
	lua_register(lua, "GetCollectionQuestAttributeQuantity", GetCollectionQuestAttributeQuantity);
	lua_register(lua, "GetCollectionQuestAttributeLabel", GetCollectionQuestAttributeLabel);
	lua_register(lua, "GetCollectionQuestQuantity", GetCollectionQuestQuantity);
	lua_register(lua, "GetCollectionQuestAttribute", GetCollectionQuestAttribute);
	lua_register(lua, "MakeInventoryContainer", MakeInventoryContainer);
	lua_register(lua, "GetInventoryTotal", GetInventoryTotal);
	lua_register(lua, "GetInventoryName", GetInventoryName);
	lua_register(lua, "GetInventoryExist", GetInventoryExist);
	lua_register(lua, "GetInventoryQuantity", GetInventoryQuantity);
	lua_register(lua, "GetInventoryItem", GetInventoryItem);
	lua_register(lua, "GetInventoryItemID", GetInventoryItemID);
	lua_register(lua, "GetInventoryItemSlot", GetInventoryItemSlot);
	lua_register(lua, "SetInventoryItemSlot", SetInventoryItemSlot);
	lua_register(lua, "MoveInventoryItem", MoveInventoryItem);
	lua_register(lua, "DeleteAllInventoryContainers", DeleteAllInventoryContainers);
	lua_register(lua, "AddInventoryItem", AddInventoryItem);
	#endif

	lua_register(lua, "SetCharacterDirectionOverride", SetCharacterDirectionOverride);
	lua_register(lua, "LimitSwimmingVerticalMovement", LimitSwimmingVerticalMovement);

	// material commands
	lua_register(lua, "SetEntityBaseColor", SetEntityBaseColor);
	lua_register(lua, "ResetEntityMaterial", ResetEntityMaterial);
	lua_register(lua, "GetEntityMeshCount", GetEntityMeshCount);
	lua_register(lua, "SetEntityMeshBaseColor", SetEntityMeshBaseColor);
	lua_register(lua, "GetEntityMeshBaseColor", GetEntityMeshBaseColor);
	lua_register(lua, "SetEntityMeshEmissiveColor", SetEntityMeshEmissiveColor);
	lua_register(lua, "GetEntityMeshEmissiveColor", GetEntityMeshEmissiveColor);
	lua_register(lua, "GetEntityBaseColor", GetEntityBaseColor);
	lua_register(lua, "SetEntityBaseAlpha", SetEntityBaseAlpha);
	lua_register(lua, "GetEntityBaseAlpha", GetEntityBaseAlpha);
	lua_register(lua, "SetEntityAlphaClipping", SetEntityAlphaClipping);
	lua_register(lua, "GetEntityAlphaClipping", GetEntityAlphaClipping);
	lua_register(lua, "SetEntityNormalStrength", SetEntityNormalStrength);
	lua_register(lua, "GetEntityNormalStrength", GetEntityNormalStrength);
	lua_register(lua, "SetEntityRoughnessStrength", SetEntityRoughnessStrength);
	lua_register(lua, "GetEntityRoughnessStrength", GetEntityRoughnessStrength);
	lua_register(lua, "SetEntityMetalnessStrength", SetEntityMetalnessStrength);
	lua_register(lua, "GetEntityMetalnessStrength", GetEntityMetalnessStrength);
	lua_register(lua, "SetEntityEmissiveColor", SetEntityEmissiveColor);
	lua_register(lua, "GetEntityEmissiveColor", GetEntityEmissiveColor);
	lua_register(lua, "SetEntityEmissiveStrength", SetEntityEmissiveStrength);
	lua_register(lua, "GetEntityEmissiveStrength", GetEntityEmissiveStrength);
	lua_register(lua, "SetEntityReflectance", SetEntityReflectance);
	lua_register(lua, "GetEntityReflectance", GetEntityReflectance);
	lua_register(lua, "SetEntityRenderBias", SetEntityRenderBias);
	lua_register(lua, "GetEntityRenderBias", GetEntityRenderBias);
	lua_register(lua, "SetEntityTransparency", SetEntityTransparency);
	lua_register(lua, "GetEntityTransparency", GetEntityTransparency);
	lua_register(lua, "SetEntityDoubleSided", SetEntityDoubleSided);
	lua_register(lua, "GetEntityDoubleSided", GetEntityDoubleSided);
	lua_register(lua, "SetEntityPlanarReflection", SetEntityPlanarReflection);
	lua_register(lua, "GetEntityPlanarReflection", GetEntityPlanarReflection);
	lua_register(lua, "SetEntityCastShadows", SetEntityCastShadows);
	lua_register(lua, "GetEntityCastShadows", GetEntityCastShadows);
	lua_register(lua, "SetEntityZDepthMode", SetEntityZDepthMode);
	lua_register(lua, "GetEntityZDepthMode", GetEntityZDepthMode);
	lua_register(lua, "SetEntityOutline", SetEntityOutline);
	lua_register(lua, "GetEntityOutline", GetEntityOutline);

	// texture commands
	lua_register(lua, "SetEntityTexture", SetEntityTexture);
	lua_register(lua, "SetEntityTextureScale", SetEntityTextureScale);
	lua_register(lua, "SetEntityTextureOffset", SetEntityTextureOffset);

	lua_register(lua, "SetWeaponArmsVisible", SetWeaponArmsVisible);
	lua_register(lua, "GetWeaponArmsVisible", GetWeaponArmsVisible);

	lua_register(lua, "GetEntityInZoneWithFilter", GetEntityInZoneWithFilter);
	lua_register(lua, "IsPointWithinZone", IsPointWithinZone);

	// In-game HUD
	lua_register(lua, "IsPlayerInGame", IsPlayerInGame);
	lua_register(lua, "SetLevelFadeoutEnabled", SetLevelFadeoutEnabled);

	// Sun
	lua_register(lua, "SetSunDirection", SetSunDirection);
	lua_register(lua, "SetSunLightingColor", SetSunLightingColor);
	lua_register(lua, "SetSunIntensity", SetSunIntensity);
	lua_register(lua, "GetSunIntensity", GetSunIntensity);

	lua_register(lua, "GetSunColorBlue", GetSunColorBlue);
	lua_register(lua, "GetSunColorGreen", GetSunColorGreen);
	lua_register(lua, "GetSunColorRed", GetSunColorRed);

	// Lighting
	lua_register(lua, "SetExposure", SetExposure);
	lua_register(lua, "GetExposure", GetExposure);
	lua_register(lua, "SetBloom", SetBloom);
	lua_register(lua, "GetBloom", GetBloom);
	lua_register(lua, "SetDepthOfField", SetDepthOfField);
	lua_register(lua, "GetDepthOfField", GetDepthOfField);
	lua_register(lua, "SetLightShafts", SetLightShafts);
	lua_register(lua, "GetLightShafts", GetLightShafts);
	lua_register(lua, "SetLensFlare", SetLensFlare);
	lua_register(lua, "GetLensFlare", GetLensFlare);
	lua_register(lua, "SetOcclusionCulling", SetOcclusionCulling);
	lua_register(lua, "SetLODMultiplier", SetLODMultiplier);
	lua_register(lua, "SetShadowsLowestLOD", SetShadowsLowestLOD);
	lua_register(lua, "SetDelayedShadows", SetDelayedShadows);
	lua_register(lua, "SetFXAA", SetFXAA);
	lua_register(lua, "SetReflections", SetReflections);
	lua_register(lua, "SetProbesLowestLOD", SetProbesLowestLOD);
	lua_register(lua, "SetReflectionsLowestLOD", SetReflectionsLowestLOD);
	lua_register(lua, "SetAnimations30Fps", SetAnimations30Fps);
	lua_register(lua, "SetMaxApparentSize", SetMaxApparentSize);
	lua_register(lua, "SetShadowResolution", SetShadowResolution);
	lua_register(lua, "SetShadowLights", SetShadowLights);
	lua_register(lua, "SetShadowCascades", SetShadowCascades);
	lua_register(lua, "SetTerrainDetail", SetTerrainDetail);
	lua_register(lua, "SetGrassSimpleLighting", SetGrassSimpleLighting);
	lua_register(lua, "GetGraphicsSettings", GetGraphicsSettings);
	lua_register(lua, "ResetGraphicsSettings", ResetGraphicsSettings);
	lua_register(lua, "SetVsync", SetVsync);
	lua_register(lua, "SetFrameRateCap", SetFrameRateCap);
	lua_register(lua, "SetRenderScale", SetRenderScale);
	lua_register(lua, "GetRenderScale", GetRenderScale);
	lua_register(lua, "SetShadowJobWait", SetShadowJobWait);
	lua_register(lua, "ResetDelayedShadows", ResetDelayedShadows);
	lua_register(lua, "GetDelayedShadows", GetDelayedShadows);
	lua_register(lua, "SetShadowRange", SetShadowRange);
	lua_register(lua, "SetSSR", SetSSR);
	lua_register(lua, "SetAO", SetAO);
	lua_register(lua, "SetWind", SetWind);
	lua_register(lua, "GetWind", GetWind);

	lua_register(lua, "SetLutTo", lua_set_lut);
	lua_register(lua, "GetLut", lua_get_lut);
	lua_register(lua, "PromptLocalOffset", PromptLocalOffset);
	lua_register(lua, "PromptGuruMeditation", PromptGuruMeditation);

	//Other effects.
	lua_register(lua, "SetGrassScale", SetGrassScale);
	lua_register(lua, "GetGrassScale", GetGrassScale);
	lua_register(lua, "SetGrassDistance", SetGrassDistance);
	lua_register(lua, "GetGrassDistance", GetGrassDistance);
	lua_register(lua, "SetGrassKillBox", SetGrassKillBox);
	lua_register(lua, "SetGrassShade", SetGrassShade);
	lua_register(lua, "GetGrassShade", GetGrassShade);
	lua_register(lua, "GetTextureSource", GetTextureSource);
	lua_register(lua, "GetEngineProbe", GetEngineProbe);
	lua_register(lua, "ClearGrassKillBox", ClearGrassKillBox);
	lua_register(lua, "SetTreeDistance", SetTreeDistance);
	lua_register(lua, "GetTreeDistance", GetTreeDistance);
	lua_register(lua, "SetTreeTransition", SetTreeTransition);
	lua_register(lua, "GetTreeTransition", GetTreeTransition);
	lua_register(lua, "SetTreeShadowCascades", SetTreeShadowCascades);
	lua_register(lua, "GetTreeShadowCascades", GetTreeShadowCascades);
	lua_register(lua, "RayTrees", RayTrees);
	lua_register(lua, "GetTreesNear", GetTreesNear);
	lua_register(lua, "GetTreeTypeName", GetTreeTypeName);
	lua_register(lua, "GetTreeCounts", GetTreeCounts);
	lua_register(lua, "GetTerrainPageStats", GetTerrainPageStats);
	lua_register(lua, "GetGameQuality", GetGameQuality);
	lua_register(lua, "GunAnimationSetFrame", GunAnimationSetFrame);
	lua_register(lua, "LoopGunAnimation", LoopGunAnimation);
	lua_register(lua, "StopGunAnimation", StopGunAnimation);
	lua_register(lua, "PlayGunAnimation", PlayGunAnimation);
	lua_register(lua, "GunAnimationPlaying", GunAnimationPlaying);
	lua_register(lua, "GetGunAnimationFramesFromName", GetGunAnimationFramesFromName);
	lua_register(lua, "SetGunAnimationSpeed", SetGunAnimationSpeed);
	lua_register(lua, "ForceGunUnderWater", ForceGunUnderWater);
	lua_register(lua, "GetGunEmissiveStrength", GetGunEmissiveStrength);
	lua_register(lua, "SetGunEmissiveStrength", SetGunEmissiveStrength);
	
}

 /*
 struct luaMessage
{
	char msgDesc[256];
	int msgInt;
	float msgFloat;
	char msgString[256];
};

luaMessage currentMessage;

int luaMessageCount = 0;
int maxLuaMessages = 0;
luaMessage** ppLuaMessages = NULL;
 */

char szLuaReturnString[1024];

 DARKLUA_API LPSTR LuaMessageDesc ( void )
 {
  	// Return string pointer	
	const char *s = currentMessage.msgDesc;

	// If input string valid
	if(s)
	{
		strcpy(szLuaReturnString, s);
	}
	else
	{
		strcpy(szLuaReturnString, "");
	}

	return GetReturnStringFromTEXTWorkString( szLuaReturnString );
 }

 DARKLUA_API int LuaMessageIndex ()
 {
	return currentMessage.msgIndex;
 }

 DARKLUA_API float LuaMessageFloat ()
 {
	return currentMessage.msgFloat;
 }

  DARKLUA_API int LuaMessageInt ()
 {
	return currentMessage.msgInt;
 }

  char szLuaMessageString[1024];

 DARKLUA_API LPSTR LuaMessageString ( void )
 {
  	// Return string pointer
	LPSTR pReturnString=NULL;
	const char *s = currentMessage.msgString;

	// If input string valid
	if(s)
	{
		strcpy(szLuaMessageString, s);
	}
	else
	{
		strcpy(szLuaMessageString, "");
	}

	return GetReturnStringFromTEXTWorkString( szLuaMessageString );
 }

 DARKLUA_API int LuaNext()
 {
	if ( luaMessageCount == 0 ) 
	{
		strcpy ( currentMessage.msgDesc, "" );
		currentMessage.msgFloat = 0.0f;
		currentMessage.msgInt = 0;
		currentMessage.msgIndex = 0;
		strcpy ( currentMessage.msgString, "" );
		return 0;
	}
	else
	{
		if (ppLuaMessages && ppLuaMessages[0]->msgDesc)
		{
			strcpy (currentMessage.msgDesc, ppLuaMessages[0]->msgDesc);
			currentMessage.msgFloat = ppLuaMessages[0]->msgFloat;
			currentMessage.msgInt = ppLuaMessages[0]->msgInt;
			currentMessage.msgIndex = ppLuaMessages[0]->msgIndex;
			strcpy (currentMessage.msgString, ppLuaMessages[0]->msgString);

			delete ppLuaMessages[0];
			ppLuaMessages[0] = NULL;

			for (int c = 1; c < luaMessageCount; c++)
				ppLuaMessages[c - 1] = ppLuaMessages[c];

			ppLuaMessages[luaMessageCount - 1] = NULL;

			luaMessageCount--;
		}
		else
		{
			// something erased all/part LUA messages array
			return 0;
		}
	}
	return 1;
 }

 DARKLUA_API void SetLuaState ( int id )
 {
	 defaultState = id;
 }

 bool checkScriptAlreadyLoaded ( int id , LPSTR pString )
 {

	 for ( int c = 0 ; c < ScriptsLoaded.size() ; c++ )
	 {
		 if ( strcmp ( pString , ScriptsLoaded[c].fileName ) == 0 )
		 {
			 if ( id == ScriptsLoaded[c].stateID )
			 {
				return true;
			 }
		 }
	 }

	
	 StringList tempStringList;
	 strcpy ( tempStringList.fileName , pString );
	 tempStringList.stateID = id;

	 ScriptsLoaded.push_back(tempStringList);

	 return false;

 }

 DARKLUA_API int LoadLua( LPSTR pString , int id )
 {

	 if ( checkScriptAlreadyLoaded ( id , pString ) ) return 0;

	if ( id <= 0 )
	{
		//MessageBox(NULL, "invalid Lua ID, must be 1 or above", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( maxLuaStates == 0 )
	{
		maxLuaStates = 100;
		ppLuaStates = new luaState*[maxLuaStates+1];

		for ( int c = 0 ; c < maxLuaStates+1 ; c++ )
			ppLuaStates[c] = NULL;
	}

	if ( id > maxLuaStates )
	{
		luaState** ppBigger = NULL;
		ppBigger = new luaState*[maxLuaStates+101];

		for ( int c = 0; c < maxLuaStates+1; c++ )
		 ppBigger [ c ] = ppLuaStates [ c ];

		delete [ ] ppLuaStates;

		ppLuaStates = ppBigger;

		for ( int c = maxLuaStates+1; c < maxLuaStates+101; c++ )
			ppLuaStates[c] = NULL;

		maxLuaStates += 100;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua State ID already in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		//return 0;

		// create new Lua state   
		lua = luaL_newstate();

		// load Lua libraries
		static const luaL_Reg lualibs[] =
		{
			{ "base", luaopen_base },
			{ NULL, NULL}
		};

		addFunctions();

		/*const luaL_Reg *lib = lualibs;
		for(; lib->func != NULL; lib++)
		{
			lib->func(lua);
			lua_settop(lua, 0);
		}*/

		luaL_openlibs(lua);
	}

    // run the Lua script
	int result = 0;

	char VirtualFilename[MAX_PATH];
	strcpy ( VirtualFilename , pString );
	LuaCheckForWorkshopFile ( VirtualFilename );

	result =  luaL_dofile(lua, VirtualFilename);

	if (result == 1 )
	{
		while(ShowCursor(TRUE) <= 0);
		SetCursorPos ( g_dwScreenWidth / 2 , g_dwScreenHeight / 2 );
		MessageBox( g_pGlob->hWnd , lua_tostring(lua, -1), "LUA ERROR!" , MB_OK | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND );
		lua_pop(lua, 1);
	}
	else
	{
		if ( ppLuaStates[id] == NULL )
			ppLuaStates[id] = new luaState;

		ppLuaStates[id]->state = lua;
	}

	return result;
 }

 DARKLUA_API int LoadLua( LPSTR pString )
 {
	int id = defaultState;


	if ( checkScriptAlreadyLoaded ( id , pString ) ) return 0;

	if ( id <= 0 )
	{
		//MessageBox(NULL, "invalid Lua ID, must be 1 or above", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( maxLuaStates == 0 )
	{
		maxLuaStates = 100;
		ppLuaStates = new luaState*[maxLuaStates+1];

		for ( int c = 0 ; c < maxLuaStates+1 ; c++ )
			ppLuaStates[c] = NULL;
	}

	if ( id > maxLuaStates )
	{
		luaState** ppBigger = NULL;
		ppBigger = new luaState*[maxLuaStates+101];

		for ( int c = 0; c < maxLuaStates+1; c++ )
		 ppBigger [ c ] = ppLuaStates [ c ];

		delete [ ] ppLuaStates;

		ppLuaStates = ppBigger;

		for ( int c = maxLuaStates+1; c < maxLuaStates+101; c++ )
			ppLuaStates[c] = NULL;

		maxLuaStates += 100;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua State ID already in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		//return 0;

		// create new Lua state   
		lua = luaL_newstate();

		// load Lua libraries
		static const luaL_Reg lualibs[] =
		{
			{ "base", luaopen_base },
			{ NULL, NULL}
		};

		addFunctions();

		/*const luaL_Reg *lib = lualibs;
		for(; lib->func != NULL; lib++)
		{
			lib->func(lua);
			lua_settop(lua, 0);
		}*/

		luaL_openlibs(lua);

	}

	/*lua_getglobal(lua, "package");
	lua_getfield(lua, -1, "path"); // get field "path" from table at top of stack (-1)
	std::string cur_path = lua_tostring(lua, -1); // grab path string from top of stack
	cur_path.append (";"); // do your path magic here
	cur_path.append("F:/TGCSHARED/fpsc-reloaded/FPS Creator Files/Files/?");
	lua_pop(lua, 1); // get rid of the string on the stack we just pushed on line 5
	lua_pushstring(lua, cur_path.c_str()); // push the new one
	lua_setfield(lua, -2, "path"); // set the field "path" in table at -2 with value at top of stack
	lua_pop(lua, 1); // get rid of package table from top of stack*/

    // run the Lua script
	int result = 0;

	char VirtualFilename[MAX_PATH];
	strcpy ( VirtualFilename , pString );
	LuaCheckForWorkshopFile ( VirtualFilename );

	result = luaL_dofile(lua, VirtualFilename);

	if (result == 1 )
	{
		while(ShowCursor(TRUE) <= 0);
		SetCursorPos ( g_dwScreenWidth / 2 , g_dwScreenHeight / 2 );
		MessageBox( g_pGlob->hWnd , lua_tostring(lua, -1), "LUA ERROR!" , MB_OK | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND );
		//MessageBox(NULL, lua_tostring(lua, -1), "LUA ERROR", MB_TOPMOST | MB_OK);
		lua_pop(lua, 1);
	}
	else
	{
		if ( ppLuaStates[id] == NULL )
			ppLuaStates[id] = new luaState;

		ppLuaStates[id]->state = lua;
	}

	return result;
 }

DARKLUA_API void LuaSetFunction( LPSTR pString , int id, int params, int results )
{

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	lua = ppLuaStates[id]->state;
	
	char* pLastOccurance =  strrchr ( pString , '\\' );
	if ( pLastOccurance )
		strcpy ( functionName, pLastOccurance+1 );
	else
		strcpy ( functionName , pString );
	functionParams = params;
	functionResults = results;
	functionStateID = id;

	// the function name 
	lua_getglobal(lua, functionName );
}

DARKLUA_API void LuaSetFunction( LPSTR pString , int params, int results )
{
	int id = defaultState;

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaSetFunction" , true );
	WriteToDebugLog ( "ID" , id );
	WriteToDebugLog ( "pString" , pString );
	WriteToDebugLog ( "params" , params );
	WriteToDebugLog ( "results" , results );
	WriteToDebugLog ( "===========" , true );
#endif

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	lua = ppLuaStates[id]->state;

	char* pLastOccurance =  strrchr ( pString , '\\' );
	if ( pLastOccurance )
		strcpy ( functionName, pLastOccurance+1 );
	else
		strcpy ( functionName , pString );
	functionParams = params;
	functionResults = results;
	functionStateID = id;

	// the function name 
	lua_getglobal(lua, functionName );
}

DARKLUA_API int LuaValidateEntityTable ( int iEntityIndex )
{
	int iValid = 0;
	int id = defaultState;
	if ( id > maxLuaStates+1 ) return 0;
	if ( ppLuaStates==NULL ) return 0;
	if ( ppLuaStates[id] == NULL ) return 0;
	lua = ppLuaStates[id]->state;
	int stacktopindex = lua_gettop (lua);
	lua_getglobal(lua, "g_Entity");
	lua_pushnumber(lua, iEntityIndex); 
	lua_gettable(lua, -2); // g_Entity[e] 
	lua_pushstring(lua, "x");
	if ( lua_istable(lua,-2) )
	{
		lua_gettable(lua, -2);  // g_Entity[e]["x"]
		if ( lua_isnumber ( lua, -1 ) || lua_isstring ( lua, -1 ) )
		{
			// table exists and the element within the table also exists as a number/string so its valid
			iValid = 1;
		}
	}
	lua_settop (lua, stacktopindex);
	return iValid;
}


DARKLUA_API void LuaCall()
{
	for ( int c = 0 ; c < FunctionsWithErrors.size() ; c++ )
	{
		if ( strcmp ( functionName , FunctionsWithErrors[c].fileName ) == 0 )
		{
			lua_pop(lua,functionParams+1);
			return;
		}
	}

	int id = functionStateID;

	int failedResults = 0;

	if ( id > maxLuaStates+1 )
	{

		//add to error list
		StringList item;
		strcpy ( item.fileName , functionName );
		FunctionsWithErrors.push_back(item);

		lua_pop(lua,functionParams+1);

		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//add to error list
		StringList item;
		strcpy ( item.fileName , functionName );
		FunctionsWithErrors.push_back(item);

		lua_pop(lua,functionParams+1);

		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaCall" , true );
	WriteToDebugLog ( "ID" , functionStateID );
	WriteToDebugLog ( "functionParams" , functionParams );
	WriteToDebugLog ( "functionResults" , functionResults );
	WriteToDebugLog ( "===========" , true );
#endif
	GG_CRASH_CONTEXT("LuaCall", "functionName=%s functionParams=%d", functionName, functionParams);

	lua = ppLuaStates[id]->state;

	//if ( functionParams == 1 )
	//	int dave = 1;

	// call the function with x arguments, return y results
	if ( lua_isfunction(lua, -(1+functionParams)) == 1 )
	{
		//lua_call(lua, functionParams, functionResults);
      if (lua_pcall(lua, functionParams, functionResults, 0) != 0)
	  {
		//add to error list
		StringList item;
		strcpy ( item.fileName , functionName );
		FunctionsWithErrors.push_back(item);

		char s[256];
		sprintf ( s , "error running function: %s", lua_tostring(lua, -1));
		//MessageBox(NULL, s, "LUA ERROR", MB_TOPMOST | MB_OK);
		while(ShowCursor(TRUE) <= 0);
		SetCursorPos ( g_dwScreenWidth / 2 , g_dwScreenHeight / 2 );
		MessageBox( g_pGlob->hWnd , lua_tostring(lua, -1), "LUA ERROR" , MB_OK | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND );
		lua_pop(lua, 1);
		failedResults = 1;
	  }
	}
	else
	{

		//add to error list
		StringList item;
		strcpy ( item.fileName , functionName );
		FunctionsWithErrors.push_back(item);

		// remove params from the stack
		lua_pop(lua,functionParams);
		failedResults = 1;

		char s[256];
		//sprintf ( s , "No function called %s" , functionName );
		//MessageBox(NULL, s, "LUA ERROR", MB_TOPMOST | MB_OK);
	}

	functionStateID = 0;
	if ( failedResults > 0 )
		lua_pop(lua, failedResults);
}

DARKLUA_API void LuaCallSilent()
{
	for ( int c = 0 ; c < FunctionsWithErrors.size() ; c++ )
	{
		if ( strcmp ( functionName , FunctionsWithErrors[c].fileName ) == 0 )
		{
			lua_pop(lua,functionParams+1);
			return;
		}
	}

	int id = functionStateID;

	int failedResults = 0;

	if ( id > maxLuaStates+1 )
	{

		//add to error list
		StringList item;
		strcpy ( item.fileName , functionName );
		FunctionsWithErrors.push_back(item);

		lua_pop(lua,functionParams+1);

		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{

		//add to error list
		StringList item;
		strcpy ( item.fileName , functionName );
		FunctionsWithErrors.push_back(item);

		lua_pop(lua,functionParams+1);

		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaCallSilent" , true );
	WriteToDebugLog ( "ID" , id );
	WriteToDebugLog ( "functionParams" , functionParams );
	WriteToDebugLog ( "functionResults" , functionResults );
	WriteToDebugLog ( "===========" , true );
#endif
	GG_CRASH_CONTEXT("LuaCall", "functionName=%s functionParams=%d", functionName, functionParams);

	lua = ppLuaStates[id]->state;

	// call the function with x arguments, return y results
	if ( lua_isfunction(lua, -(1+functionParams)) == 1 )
	{
		//lua_call(lua, functionParams, functionResults);
      if (lua_pcall(lua, functionParams, functionResults, 0) != 0)
	  {
		//add to error list
		StringList item;
		strcpy ( item.fileName , functionName );
		FunctionsWithErrors.push_back(item);

		char s[256];
		//sprintf ( s , "error running function: %s", lua_tostring(lua, -1));
		//MessageBox(NULL, s, "LUA ERROR", MB_TOPMOST | MB_OK);
		failedResults = 1;
	  }
	}
	else
	{

		//add to error list
		StringList item;
		strcpy ( item.fileName , functionName );
		FunctionsWithErrors.push_back(item);

		// remove params from the stack
		lua_pop(lua,functionParams);
		failedResults = 1;
	}

	functionStateID = 0;
	if ( failedResults > 0 )
		lua_pop(lua, failedResults);

}

 DARKLUA_API void CloseLua( int id )
 {
	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates[id] != NULL )
	{
		// close the Lua state
		lua_close(ppLuaStates[id]->state);
		delete ppLuaStates[id];
		ppLuaStates[id] = NULL;
	}
	//else
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);

 }

  DARKLUA_API void CloseLua()
 {
	int id = defaultState;
	
	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates[id] != NULL )
	{
		// close the Lua state
		lua_close(ppLuaStates[id]->state);
		delete ppLuaStates[id];
		ppLuaStates[id] = NULL;
	}
	//else
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);

 }

  DARKLUA_API void CloseLuaSilent( int id )
 {
	if ( id > maxLuaStates+1 )
	{
		return;
	}

	if ( ppLuaStates[id] != NULL )
	{
		// close the Lua state
		lua_close(ppLuaStates[id]->state);
		delete ppLuaStates[id];
		ppLuaStates[id] = NULL;
	}

 }

 DARKLUA_API void LuaReset ()
 {
	// Close all states, silently
	if ( ppLuaStates )
	{
		for ( int c = 0 ; c < maxLuaStates+1; c++ )
		{
			if ( ppLuaStates[c] != NULL )
			{
				CloseLuaSilent (c);
				ppLuaStates[c] = NULL;
			}
		}
		delete[] ppLuaStates;
		ppLuaStates = NULL;
		maxLuaStates = 0;
	}

	// Empty Message Queue
	if ( ppLuaMessages )
	{
		for ( int c = 0 ; c < maxLuaMessages; c++ )
		{
			if ( ppLuaMessages[c] != NULL )
			{
				delete ppLuaMessages[c];
				ppLuaMessages[c] = NULL;
			}
		}
	}

	// reset messaging
	strcpy ( currentMessage.msgDesc, "" );
	currentMessage.msgFloat = 0.0f;
	currentMessage.msgInt = 0;
	currentMessage.msgIndex = 0;
	strcpy ( currentMessage.msgString, "" );

	//Reset already loaded list
	ScriptsLoaded.clear();

	//Reset error list
	FunctionsWithErrors.clear();

	// 050416 - delete any sprites created inside LUA scripting
	for ( int c = g.LUASpriteoffset ; c <= g.LUASpriteoffsetMax ; c++ )
		if ( SpriteExist ( c ) == 1 )
			DeleteSprite ( c );

	// restore state default
	defaultState = 1;
 }

 DARKLUA_API int LuaExecute ( LPSTR pString , int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	lua = ppLuaStates[id]->state;

    // run the Lua script string
	int a = 0;

	GG_CRASH_CONTEXT("LuaExecute", "pString=%s id=%d", pString, id);

	a = luaL_loadbuffer(lua, pString, strlen(pString), pString) ||	lua_pcall(lua, 0, 0, 0);
	if (a) 
	{
	  //MessageBox(NULL, lua_tostring(lua, -1), "LUA ERROR", MB_TOPMOST | MB_OK);
	  lua_pop(lua, 1);  /* pop error message from the stack */
	}

	// Return 1 for success, like dbpro styles
	if ( a == 0 )
		a = 1;
	else
		a = 0;

	return a;
 }

 DARKLUA_API int LuaExecute ( LPSTR pString )
 {
	if ( ppLuaStates == NULL ) return 1;

	int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	lua = ppLuaStates[id]->state;

    // run the Lua script string
	int a = 0;

	GG_CRASH_CONTEXT("LuaExecute", "pString=%s", pString);

	a = luaL_loadbuffer(lua, pString, strlen(pString), pString) ||	lua_pcall(lua, 0, 0, 0);
	if (a) 
	{
	  //MessageBox(NULL, lua_tostring(lua, -1), "LUA ERROR", MB_TOPMOST | MB_OK);
	  lua_pop(lua, 1);  /* pop error message from the stack */
	}

	// Return 1 for success, like dbpro styles
	if ( a == 0 )
		a = 1;
	else
		a = 0;

	return a;
 }

 DARKLUA_API int LuaGetInt ( LPSTR pString , int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	lua = ppLuaStates[id]->state;
    lua_getglobal(lua, pString);

	/*if (!lua_isnumber(L, -1))
    {
        MessageBox(NULL, "Variable is not a number", "LUA ERROR", MB_TOPMOST | MB_OK);
        return 0;
    }*/

	int ret = (int)lua_tonumber(lua, -1);
	lua_pop(lua,1);

    return ret;
 }

  DARKLUA_API int LuaGetInt ( LPSTR pString )
 {
	 int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	lua = ppLuaStates[id]->state;
    lua_getglobal(lua, pString);

	/*if (!lua_isnumber(L, -1))
    {
        MessageBox(NULL, "Variable is not a number", "LUA ERROR", MB_TOPMOST | MB_OK);
        return 0;
    }*/

	int ret = (int)lua_tonumber(lua, -1);
	lua_pop(lua,1);

    return ret;
 }

  DARKLUA_API int LuaReturnInt ( int id )
 {
	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( lua_gettop(lua) == 0 )
	{
		return 0;
	}

	lua = ppLuaStates[id]->state;

	/*if (!lua_isnumber(L, -1))
    {
        MessageBox(NULL, "Variable is not a number", "LUA ERROR", MB_TOPMOST | MB_OK);
        return 0;
    }*/

	int ret = (int)lua_tonumber(lua, -1);
	lua_pop(lua,1);

    return ret;
 }

 DARKLUA_API int LuaReturnInt ( void )
 {
	int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( lua_gettop(lua) == 0 )
	{
		return 0;
	}

	lua = ppLuaStates[id]->state;

	/*if (!lua_isnumber(L, -1))
    {
        MessageBox(NULL, "Variable is not a number", "LUA ERROR", MB_TOPMOST | MB_OK);
        return 0;
    }*/

	int ret = (int)lua_tonumber(lua, -1);
	lua_pop(lua,1);

    return ret;
 }

 DARKLUA_API float LuaGetFloat ( LPSTR pString , int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	lua = ppLuaStates[id]->state;
    lua_getglobal(lua, pString);

	/*if (!lua_isnumber(L, -1))
    {
        MessageBox(NULL, "Variable is not a number", "LUA ERROR", MB_TOPMOST | MB_OK);
        return 0;
    }*/

	float fValue = (float)lua_tonumber(lua, -1);
	lua_pop(lua,1);
	return fValue;
 }

 DARKLUA_API float LuaGetFloat ( LPSTR pString )
 {
	 int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	lua = ppLuaStates[id]->state;
    lua_getglobal(lua, pString);

	/*if (!lua_isnumber(L, -1))
    {
        MessageBox(NULL, "Variable is not a number", "LUA ERROR", MB_TOPMOST | MB_OK);
        return 0;
    }*/

	float fValue = (float)lua_tonumber(lua, -1);
	lua_pop(lua,1);
	return fValue;
 }

 DARKLUA_API float LuaReturnFloat ( int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( lua_gettop(lua) == 0 )
	{
		return 0;
	}

	lua = ppLuaStates[id]->state;

	/*if (!lua_isnumber(L, -1))
    {
        MessageBox(NULL, "Variable is not a number", "LUA ERROR", MB_TOPMOST | MB_OK);
        return 0;
    }*/

	float fValue = (float)lua_tonumber(lua, -1);
	lua_pop(lua,1);
	return fValue;
 }

 DARKLUA_API float LuaReturnFloat ( void )
 {
	 int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( lua_gettop(lua) == 0 )
	{
		return 0;
	}

	lua = ppLuaStates[id]->state;

	/*if (!lua_isnumber(L, -1))
    {
        MessageBox(NULL, "Variable is not a number", "LUA ERROR", MB_TOPMOST | MB_OK);
        return 0;
    }*/

	float fValue = (float)lua_tonumber(lua, -1);
	lua_pop(lua,1);
	return fValue;
 }

 DARKLUA_API void LuaGetString(LPSTR pString, LPSTR pDestStr)
 {
	 int id = defaultState;
	 if (id > maxLuaStates + 1)
	 {
		 return;
	 }
	 if (ppLuaStates[id] == NULL)
	 {
		 return;
	 }

	 lua = ppLuaStates[id]->state;
	 lua_getglobal(lua, pString);

	 const char* pValue = lua_tostring(lua, -1);
	 if (pDestStr && pValue)
	 {
		 strcpy(pDestStr, pValue);
	 }
	 lua_pop(lua, 1);
 }

 DARKLUA_API void LuaSetInt ( LPSTR pString , int value, int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushnumber( lua, (lua_Number)value );
	lua_setglobal( lua, pString );

 }

  DARKLUA_API void LuaSetInt ( LPSTR pString , int value )
 {
	 int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushnumber( lua, (lua_Number)value );
	lua_setglobal( lua, pString );

 }

 DARKLUA_API void LuaPushInt ( int value, int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushnumber( lua, (lua_Number)value );

 }

 DARKLUA_API void LuaPushInt ( int value )
 {
	 int id = defaultState;

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaPushInt" , true );
	WriteToDebugLog ( "ID" , id );
	WriteToDebugLog ( "value" , value );
	WriteToDebugLog ( "===========" , true );
#endif

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushnumber( lua, (lua_Number)value );

 }

 DARKLUA_API void LuaSetFloat ( LPSTR pString , float value, int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushnumber( lua, (lua_Number)value );
	lua_setglobal( lua, pString );
 }

 DARKLUA_API void LuaSetFloat ( LPSTR pString , float value )
 {
	int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushnumber( lua, (lua_Number)value );
	lua_setglobal( lua, pString );
 }

 DARKLUA_API void LuaPushFloat ( float value, int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushnumber( lua, (lua_Number)value );

 }

  DARKLUA_API void LuaPushFloat ( float value )
 {
	 int id = defaultState;

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaPushFloat" , true );
	WriteToDebugLog ( "ID" , id );
	WriteToDebugLog ( "value" , value );
	WriteToDebugLog ( "===========" , true );
#endif

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushnumber( lua, (lua_Number)value );

 }
	
 DARKLUA_API void LuaSetString ( LPSTR pString , LPSTR pStringValue, int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushstring( lua, pStringValue );
	lua_setglobal( lua, pString );
 }

 DARKLUA_API void LuaSetString ( LPSTR pString , LPSTR pStringValue )
 {
	int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushstring( lua, pStringValue );
	lua_setglobal( lua, pString );
 }


 DARKLUA_API void LuaPushString ( LPSTR pStringValue, int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushstring( lua, pStringValue );

 }

 DARKLUA_API void LuaPushString ( LPSTR pStringValue )
 {
	 int id = defaultState;

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaPushString" , true );
	WriteToDebugLog ( "ID" , id );
	WriteToDebugLog ( "pStringValue" , pStringValue );
	WriteToDebugLog ( "===========" , true );
#endif

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return;
	}

	lua = ppLuaStates[id]->state;
	lua_pushstring( lua, pStringValue );

 }

 DARKLUA_API int LuaArrayInt ( LPSTR pString , int id )
 {
	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	if ( ppLuaStates==NULL ) return 0;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	lua = ppLuaStates[id]->state;

	//
	char str[512];
	strcpy ( str , pString);
	char * pch;
	pch = strstr (str,"->");
	while (pch)
	{
		if (pch) strncpy (pch,"..",2);
		pch = strstr (str,"->");
	}
	//
	//char str[512];
	//strcpy ( str , pString);
	//char * pch;
	//MessageBox(NULL, str, "str =", NULL);
	bool foundFunction = false;	
	pch = NULL;
	int offset = -1;
	pch = strtok (str,".");
	char lastString[256];
	int lastNumber;
	int lastWas= 0;
	while (pch != NULL)
	{
		if (!foundFunction)
		{
			foundFunction = true;
			
			//sprintf ( errorString , "Array = %s" , pch );
			lua_getglobal(lua, pch );
			strcpy ( lastString, pch );
			//MessageBox(NULL, errorString , "" , NULL);
			if ( !lua_istable(lua, offset) )
			{
				lua_pop(lua,-offset);
				sprintf ( errorString , "%s is not a Lua Table" , pch );
				MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
				return 0;
			}
		}
		else
		{
			if (lastWas == 1)
			{

				lua_getfield ( lua , offset , lastString );

				if ( !lua_istable(lua, offset) )
				{
					lua_pop(lua,-offset);
					sprintf ( errorString , "previous field to %s does not exist in table" , pch );
					MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
					return 0;
				}
			}
			else if ( lastWas == 2 )
			{
				lua_rawgeti ( lua, offset , lastNumber );
			}

			lastWas = 1;

			if ( pch[0] == '#' )
			{
				char szString[256];
				strcpy ( szString , pch );
				memcpy ( &szString [ 0 ], &szString [ 1 ], strlen ( szString ) - 1 );
				szString [ strlen ( szString ) - 1 ] = 0;

				int num = atoi(szString);
				lastNumber = num;
				lastWas = 2;
			}
			else
				strcpy ( lastString, pch );
			
		}

		pch = strtok (NULL, ".");
	}

	if ( lastWas != 2 )
	{
		lua_pushstring(lua, lastString );
		offset--;
		lua_gettable(lua, offset);
	}
	else
	{
		lua_rawgeti ( lua, offset , lastNumber );
	}

	int ret = (int)lua_tonumber(lua, -1);
	lua_pop(lua,-(++offset));
	return ret;

 }

  DARKLUA_API int LuaArrayInt ( LPSTR pString )
 {
	 int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	if ( ppLuaStates==NULL ) return 0;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	lua = ppLuaStates[id]->state;

	//
	char str[512];
	strcpy ( str , pString);
	char * pch;
	pch = strstr (str,"->");
	while (pch)
	{
		if (pch) strncpy (pch,"..",2);
		pch = strstr (str,"->");
	}
	//
	//char str[512];
	//strcpy ( str , pString);
	//char * pch;
	//MessageBox(NULL, str, "str =", NULL);
	bool foundFunction = false;	
	pch = NULL;
	int offset = -1;
	pch = strtok (str,".");
	char lastString[256];
	int lastNumber;
	int lastWas= 0;
	while (pch != NULL)
	{
		if (!foundFunction)
		{
			foundFunction = true;
			
			//sprintf ( errorString , "Array = %s" , pch );
			lua_getglobal(lua, pch );
			strcpy ( lastString, pch );
			//MessageBox(NULL, errorString , "" , NULL);
			if ( !lua_istable(lua, offset) )
			{
				lua_pop(lua,-offset);
				sprintf ( errorString , "%s is not a Lua Table" , pch );
				MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
				return 0;
			}
		}
		else
		{
			if (lastWas == 1)
			{

				lua_getfield ( lua , offset , lastString );

				if ( !lua_istable(lua, offset) )
				{
					lua_pop(lua,-offset);
					sprintf ( errorString , "previous field to %s does not exist in table" , pch );
					MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
					return 0;
				}
			}
			else if ( lastWas == 2 )
			{
				lua_rawgeti ( lua, offset , lastNumber );
			}

			lastWas = 1;

			if ( pch[0] == '#' )
			{
				char szString[256];
				strcpy ( szString , pch );
				memcpy ( &szString [ 0 ], &szString [ 1 ], strlen ( szString ) - 1 );
				szString [ strlen ( szString ) - 1 ] = 0;

				int num = atoi(szString);
				lastNumber = num;
				lastWas = 2;
			}
			else
				strcpy ( lastString, pch );
			
		}

		pch = strtok (NULL, ".");
	}

	if ( lastWas != 2 )
	{
		lua_pushstring(lua, lastString );
		offset--;
		lua_gettable(lua, offset);
	}
	else
	{
		lua_rawgeti ( lua, offset , lastNumber );
	}

	int ret = (int)lua_tonumber(lua, -1);
	lua_pop(lua,-(++offset));
	return ret;

 }

 DARKLUA_API float LuaArrayFloat ( LPSTR pString , int id )
 {

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	if ( ppLuaStates==NULL ) return 0;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	lua = ppLuaStates[id]->state;

	//
	char str[512];
	strcpy ( str , pString);
	char * pch;
	pch = strstr (str,"->");
	while (pch)
	{
		if (pch) strncpy (pch,"..",2);
		pch = strstr (str,"->");
	}
	//
	//char str[512];
	//strcpy ( str , pString);
	//char * pch;
	//MessageBox(NULL, str, "str =", NULL);
	bool foundFunction = false;	
	pch = NULL;
	int offset = -1;
	pch = strtok (str,".");
	char lastString[256];
	int lastNumber;
	int lastWas= 0;
	while (pch != NULL)
	{
		if (!foundFunction)
		{
			foundFunction = true;
			
			//sprintf ( errorString , "Array = %s" , pch );
			lua_getglobal(lua, pch );
			strcpy ( lastString, pch );
			//MessageBox(NULL, errorString , "" , NULL);
			if ( !lua_istable(lua, offset) )
			{
				lua_pop(lua,-offset);
				sprintf ( errorString , "%s is not a Lua Table" , pch );
				MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
				return 0;
			}
		}
		else
		{
			if (lastWas == 1)
			{

				lua_getfield ( lua , offset , lastString );

				if ( !lua_istable(lua, offset) )
				{
					lua_pop(lua,-offset);
					sprintf ( errorString , "previous field to %s does not exist in table" , pch );
					MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
					return 0;
				}
			}
			else if ( lastWas == 2 )
			{
				lua_rawgeti ( lua, offset , lastNumber );
			}

			lastWas = 1;

			if ( pch[0] == '#' )
			{
				char szString[256];
				strcpy ( szString , pch );
				memcpy ( &szString [ 0 ], &szString [ 1 ], strlen ( szString ) - 1 );
				szString [ strlen ( szString ) - 1 ] = 0;

				int num = atoi(szString);
				lastNumber = num;
				lastWas = 2;
			}
			else
				strcpy ( lastString, pch );
			
		}

		pch = strtok (NULL, ".");
	}

	if ( lastWas != 2 )
	{
		lua_pushstring(lua, lastString );
		offset--;
		lua_gettable(lua, offset);
	}
	else
	{
		lua_rawgeti ( lua, offset , lastNumber );
	}

	float fValue = (float)lua_tonumber(lua, -1);
	lua_pop(lua,1);
	return fValue;

 }

  DARKLUA_API float LuaArrayFloat ( LPSTR pString )
 {
	 int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	if ( ppLuaStates==NULL ) return 0;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	lua = ppLuaStates[id]->state;

	//
	char str[512];
	strcpy ( str , pString);
	char * pch;
	pch = strstr (str,"->");
	while (pch)
	{
		if (pch) strncpy (pch,"..",2);
		pch = strstr (str,"->");
	}
	//
	//char str[512];
	//strcpy ( str , pString);
	//char * pch;
	//MessageBox(NULL, str, "str =", NULL);
	bool foundFunction = false;	
	pch = NULL;
	int offset = -1;
	pch = strtok (str,".");
	char lastString[256];
	int lastNumber;
	int lastWas= 0;
	while (pch != NULL)
	{
		if (!foundFunction)
		{
			foundFunction = true;
			
			//sprintf ( errorString , "Array = %s" , pch );
			lua_getglobal(lua, pch );
			strcpy ( lastString, pch );
			//MessageBox(NULL, errorString , "" , NULL);
			if ( !lua_istable(lua, offset) )
			{
				lua_pop(lua,-offset);
				sprintf ( errorString , "%s is not a Lua Table" , pch );
				MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
				return 0;
			}
		}
		else
		{
			if (lastWas == 1)
			{

				lua_getfield ( lua , offset , lastString );

				if ( !lua_istable(lua, offset) )
				{
					lua_pop(lua,-offset);
					sprintf ( errorString , "previous field to %s does not exist in table" , pch );
					MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
					return 0;
				}
			}
			else if ( lastWas == 2 )
			{
				lua_rawgeti ( lua, offset , lastNumber );
			}

			lastWas = 1;

			if ( pch[0] == '#' )
			{
				char szString[256];
				strcpy ( szString , pch );
				memcpy ( &szString [ 0 ], &szString [ 1 ], strlen ( szString ) - 1 );
				szString [ strlen ( szString ) - 1 ] = 0;

				int num = atoi(szString);
				lastNumber = num;
				lastWas = 2;
			}
			else
				strcpy ( lastString, pch );
			
		}

		pch = strtok (NULL, ".");
	}

	if ( lastWas != 2 )
	{
		lua_pushstring(lua, lastString );
		offset--;
		lua_gettable(lua, offset);
	}
	else
	{
		lua_rawgeti ( lua, offset , lastNumber );
	}

	float fValue = (float)lua_tonumber(lua, -1);
	lua_pop(lua,1);
	return fValue;

 }

 DARKLUA_API LPSTR LuaArrayString ( LPSTR pString , int id )
 {
	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	if ( ppLuaStates==NULL ) return 0;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	lua = ppLuaStates[id]->state;

	//
	char str[512];
	strcpy ( str , pString);
	char * pch;
	pch = strstr (str,"->");
	while (pch)
	{
		if (pch) strncpy (pch,"..",2);
		pch = strstr (str,"->");
	}
	//
	//char str[512];
	//strcpy ( str , pString);
	//char * pch;
	//MessageBox(NULL, str, "str =", NULL);
	bool foundFunction = false;	
	pch = NULL;
	int offset = -1;
	pch = strtok (str,".");
	char lastString[256];
	int lastNumber;
	int lastWas= 0;
	while (pch != NULL)
	{
		if (!foundFunction)
		{
			foundFunction = true;
			
			//sprintf ( errorString , "Array = %s" , pch );
			lua_getglobal(lua, pch );
			strcpy ( lastString, pch );
			//MessageBox(NULL, errorString , "" , NULL);
			if ( !lua_istable(lua, offset) )
			{
				lua_pop(lua,-offset);
				sprintf ( errorString , "%s is not a Lua Table" , pch );
				MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
				return 0;
			}
		}
		else
		{
			if (lastWas == 1)
			{

				lua_getfield ( lua , offset , lastString );

				if ( !lua_istable(lua, offset) )
				{
					lua_pop(lua,-offset);
					sprintf ( errorString , "previous field to %s does not exist in table" , pch );
					MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
					return 0;
				}
			}
			else if ( lastWas == 2 )
			{
				lua_rawgeti ( lua, offset , lastNumber );
			}

			lastWas = 1;

			if ( pch[0] == '#' )
			{
				char szString[256];
				strcpy ( szString , pch );
				memcpy ( &szString [ 0 ], &szString [ 1 ], strlen ( szString ) - 1 );
				szString [ strlen ( szString ) - 1 ] = 0;

				int num = atoi(szString);
				lastNumber = num;
				lastWas = 2;
			}
			else
				strcpy ( lastString, pch );
			
		}

		pch = strtok (NULL, ".");
	}

	if ( lastWas != 2 )
	{
		lua_pushstring(lua, lastString );
		offset--;
		lua_gettable(lua, offset);
	}
	else
	{
		lua_rawgeti ( lua, offset , lastNumber );
	}

  	// Return string pointer
	LPSTR pReturnString=NULL;
	const char *s = lua_tostring(lua, -1);
	lua_pop(lua,1);

	// If input string valid
	if(s)
	{
		// Create a new string and copy input string to it
		DWORD dwSize=strlen( s );
		g_pGlob->CreateDeleteString ( (char**)&pReturnString, dwSize+1 );
		strcpy(pReturnString, s);
	}
	else
	{
		return NULL;
	}

	return pReturnString;

 }

 DARKLUA_API LPSTR LuaArrayString ( LPSTR pString )
 {
	 int id = defaultState;

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	if ( ppLuaStates==NULL ) return 0; 
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", NULL);
		return 0;
	}

	lua = ppLuaStates[id]->state;

	//
	char str[512];
	strcpy ( str , pString);
	char * pch;
	pch = strstr (str,"->");
	while (pch)
	{
		if (pch) strncpy (pch,"..",2);
		pch = strstr (str,"->");
	}
	//
	//char str[512];
	//strcpy ( str , pString);
	//char * pch;
	//MessageBox(NULL, str, "str =", NULL);
	bool foundFunction = false;	
	pch = NULL;
	int offset = -1;
	pch = strtok (str,".");
	char lastString[256];
	int lastNumber;
	int lastWas= 0;
	while (pch != NULL)
	{
		if (!foundFunction)
		{
			foundFunction = true;
			
			//sprintf ( errorString , "Array = %s" , pch );
			lua_getglobal(lua, pch );
			strcpy ( lastString, pch );
			//MessageBox(NULL, errorString , "" , NULL);
			if ( !lua_istable(lua, offset) )
			{
				lua_pop(lua,-offset);
				sprintf ( errorString , "%s is not a Lua Table" , pch );
				MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
				return 0;
			}
		}
		else
		{
			if (lastWas == 1)
			{

				lua_getfield ( lua , offset , lastString );

				if ( !lua_istable(lua, offset) )
				{
					lua_pop(lua,-offset);
					sprintf ( errorString , "previous field to %s does not exist in table" , pch );
					MessageBox(NULL, errorString , "" , MB_TOPMOST | MB_OK);
					return 0;
				}
			}
			else if ( lastWas == 2 )
			{
				lua_rawgeti ( lua, offset , lastNumber );
			}

			lastWas = 1;

			if ( pch[0] == '#' )
			{
				char szString[256];
				strcpy ( szString , pch );
				memcpy ( &szString [ 0 ], &szString [ 1 ], strlen ( szString ) - 1 );
				szString [ strlen ( szString ) - 1 ] = 0;

				int num = atoi(szString);
				lastNumber = num;
				lastWas = 2;
			}
			else
				strcpy ( lastString, pch );
			
		}

		pch = strtok (NULL, ".");
	}

	if ( lastWas != 2 )
	{
		lua_pushstring(lua, lastString );
		offset--;
		lua_gettable(lua, offset);
	}
	else
	{
		lua_rawgeti ( lua, offset , lastNumber );
	}

  	// Return string pointer
	LPSTR pReturnString=NULL;
	const char *s = lua_tostring(lua, -1);
	lua_pop(lua,1);

	// If input string valid
	if(s)
	{
		// Create a new string and copy input string to it
		DWORD dwSize=strlen( s );
		g_pGlob->CreateDeleteString ( (char**)&pReturnString, dwSize+1 );
		strcpy(pReturnString, s);
	}
	else
	{
		return NULL;
	}

	return pReturnString;
 }

// This is the constructor of a class that has been exported.
// see DarkLUA.h for the class definition
CDarkLUA::CDarkLUA()
{
	return;
}

///////////////////////////////////////////////////////////

bool LuaCheckForWorkshopFile ( LPSTR VirtualFilename)
{
	if ( !VirtualFilename ) return false;
	if ( strlen ( VirtualFilename ) < 3 ) return false;

	char* tempCharPointerCheck = NULL;
	tempCharPointerCheck = strrchr( VirtualFilename, '\\' );
	if ( tempCharPointerCheck == VirtualFilename+strlen(VirtualFilename)-1 ) return false;
	if ( VirtualFilename[0] == '.' ) return false;
	if ( strstr ( VirtualFilename , ".fpm" ) ) return false;

	// encrypted file check
	char szEncryptedFilename[_MAX_PATH];
	char szEncryptedFilenameFolder[_MAX_PATH];
	char* tempCharPointer = NULL;
	strcpy ( szEncryptedFilenameFolder, VirtualFilename );

	// replace and forward slashes with backslash
	for ( int c = 0 ; c < strlen(szEncryptedFilenameFolder); c++ )
	{
		if ( szEncryptedFilenameFolder[c] == '/' ) 
			szEncryptedFilenameFolder[c] = '\\';
	}

	tempCharPointer = strrchr( szEncryptedFilenameFolder, '\\' );
	if ( tempCharPointer && tempCharPointer != szEncryptedFilenameFolder+strlen(szEncryptedFilenameFolder)-1 )
	{
		tempCharPointer[0] = 0;
		sprintf ( szEncryptedFilename , "%s\\_e_%s" , szEncryptedFilenameFolder , tempCharPointer+1 );
	}
	else
	{
		sprintf ( szEncryptedFilename , "_e_%s" , szEncryptedFilenameFolder );
	}
	
	if ( GG_FileExists(szEncryptedFilename) )
	{
		strcpy ( VirtualFilename , szEncryptedFilename );
		return true;
	}
	// end of encrypted file check

	// Workshop handling
	#ifdef PHOTONMP
	#else
		char szWorkshopFilename[_MAX_PATH];
		char szWorkshopFilenameFolder[_MAX_PATH];
		char szWorkShopItemPath[_MAX_PATH];
		SteamGetWorkshopItemPathDLL(szWorkShopItemPath);
		//strcpy ( szWorkShopItemPath,"D:\\Games\\Steam\\steamapps\\workshop\\content\\266310\\378822626");
		// If the string is empty then there is no active workshop item, so we can return
		if ( strcmp ( szWorkShopItemPath , "" ) == 0 ) return false;
		tempCharPointer = NULL;
		strcpy ( szWorkshopFilenameFolder, VirtualFilename );

		// only check if the workshop item path isnt blank
		if ( strcmp ( szWorkShopItemPath , "" ) )
		{
			// replace and forward slashes with backslash
			for ( unsigned int c = 0 ; c < strlen(szWorkshopFilenameFolder); c++ )
			{
				if ( szWorkshopFilenameFolder[c] == '/' )
					szWorkshopFilenameFolder[c] = '\\';
			}

			// strip off any path to files folder
			bool found = true;
			while ( found )
			{
			char* stripped = strstr ( szWorkshopFilenameFolder , "Files\\" );
			if ( !stripped )
				stripped = strstr ( szWorkshopFilenameFolder , "files\\" );

			if ( stripped )
				strcpy ( szWorkshopFilenameFolder , stripped+6 );
			else
				found = false;
			}

			bool last = false;
			char tempstring[MAX_PATH];
			strcpy ( tempstring, szWorkshopFilenameFolder);
			strcpy ( szWorkshopFilenameFolder , "" );
			// replace and forward slashes with backslash
			for ( unsigned int c = 0 ; c < strlen(tempstring); c++ )
			{
				if ( tempstring[c] == '/' || tempstring[c] == '\\' ) 
				{
					if ( last == false )
					{
						strcat ( szWorkshopFilenameFolder , "_" );
						last = true;
					}
				}
				else
				{
					strcat ( szWorkshopFilenameFolder , " " );
					szWorkshopFilenameFolder[strlen(szWorkshopFilenameFolder)-1] = tempstring[c];
					last = false;
				}
			}

			//NEED TO CHECK IF THE FILE EXISTS FIRST, IF IT DOES WE COPY IT
			char szTempName[_MAX_PATH];
			strcpy ( szTempName , szWorkShopItemPath );
			strcat ( szTempName , "\\" );
			strcat ( szTempName , szWorkshopFilenameFolder );

			if ( GG_FileExists(szTempName) )
			{
				int szTempNamelength = strlen(szTempName);
				int virtualfilelength = strlen(VirtualFilename);				
				strcpy ( VirtualFilename , szTempName );
				return true;
			}
			else // check for encrypted version
			{
				char* tempCharPointer = NULL;

				tempCharPointer = strrchr( szTempName, '\\' );
				if ( tempCharPointer && tempCharPointer != szTempName+strlen(szTempName)-1 )
				{
					tempCharPointer[0] = 0;
					sprintf ( szWorkshopFilename , "%s\\_w_%s" , szTempName , tempCharPointer+1 );
				}
				else
				{
					sprintf ( szWorkshopFilename , "_w_%s" , szTempName );
				}
				
				if ( GG_FileExists(szWorkshopFilename) )
				{
					strcpy ( VirtualFilename , szWorkshopFilename );
					return true;
				}
			}
		}
	#endif
	return false;
}


#ifdef LUA_DO_DEBUG
FILE* g_fpDebug = NULL;

void OpenDebugLog ( char* szFile )
{
 char szFileOpen [ 256 ] = "";
 sprintf ( szFileOpen, "f:\\%s", szFile );
 g_fpDebug = GG_fopen ( szFileOpen, "wt" );
}

void WriteToDebugLog ( char* szMessage, bool bNewLine )
{
 if ( !g_fpDebug )
  OpenDebugLog ( "log.txt" );
 
 if ( !g_fpDebug )
  return;

 char szOut [ 256 ] = "";

 if ( bNewLine )
  sprintf ( szOut, "%s\n", szMessage );
 else
  sprintf ( szOut, "%s", szMessage );

 fwrite ( szOut, strlen ( szOut ) * sizeof ( char ), 1, g_fpDebug );
 fflush ( g_fpDebug );
}

void WriteToDebugLog ( char* szMessage, int i )
{
 if ( !g_fpDebug )
  OpenDebugLog ( "log.txt" );
 
 if ( !g_fpDebug )
  return;

 char szOut [ 256 ] = "";

 if ( 1 )
  sprintf ( szOut, "%s = %i\n", szMessage , i );


 fwrite ( szOut, strlen ( szOut ) * sizeof ( char ), 1, g_fpDebug );
 fflush ( g_fpDebug );
}

void WriteToDebugLog ( char* szMessage, float f )
{
 if ( !g_fpDebug )
  OpenDebugLog ( "log.txt" );
 
 if ( !g_fpDebug )
  return;

 char szOut [ 256 ] = "";

 if ( 1 )
  sprintf ( szOut, "%s = %f\n", szMessage , f );


 fwrite ( szOut, strlen ( szOut ) * sizeof ( char ), 1, g_fpDebug );
 fflush ( g_fpDebug );
}

void WriteToDebugLog ( char* szMessage, char* s )
{
 if ( !g_fpDebug )
  OpenDebugLog ( "log.txt" );
 
 if ( !g_fpDebug )
  return;

 char szOut [ 256 ] = "";

 if ( 1 )
  sprintf ( szOut, "%s = %s\n", s );


 fwrite ( szOut, strlen ( szOut ) * sizeof ( char ), 1, g_fpDebug );
 fflush ( g_fpDebug );
}

void CloseDebugLog ( void )
{
 fclose ( g_fpDebug );
}
#endif