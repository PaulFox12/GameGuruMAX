int SetSunIntensity(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float intensity = lua_tonumber(L, 1);
	t.visuals.SunIntensity_f = intensity;
	WickedCall_SetSunColors(t.visuals.SunRed_f, t.visuals.SunGreen_f, t.visuals.SunBlue_f, t.visuals.SunIntensity_f, 1.0f, 1.0f);
	return 1;
}

// ★ GGMAX 3.38: ported from DX11 b80d0d53 ("PE: Added LUA GetSunColorBlue(), GetSunColorRed(),
// GetSunColorGreen() (4 Necrym59)"). These were the ONLY Lua commands the DX11 tree had that this
// one lacked, apart from ForceMouseXYClick - the two APIs are otherwise identical, so the behaviour
// scripts shipped alongside them could not run here until these existed. Straight readers of the
// same t.visuals fields SetSunLightingColor already writes; no renderer involvement.
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
	t.luaglobal.gurumeditationprompttime = MAXTimer();
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

//PE: USE - SetLutTo("editors\\lut\\sephia.png")
//PE: USE - string = GetLut()
void Wicked_Update_Visuals(void* voidvisual);
void Wicked_Update_LUT(void* voidvisual);
int lua_get_lut(lua_State* L)
{
	// SetLutTo writes t.gamevisuals (in a test game only that, so the editor keeps its own LUT), so read it back from there
	std::string retstr = t.gamevisuals.ColorGradingLUT.Get();
	if(retstr.length() > 0)
		lua_pushstring(lua2, retstr.c_str());
	else
		lua_pushstring(lua2, "None");
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
	lua_register(lua2, "SendMessage_startgame", SendMessage_startgame);
	lua_register(lua2, "SendMessage_loadgame", SendMessage_loadgame);
	lua_register(lua2, "SendMessage_savegame", SendMessage_savegame);
	lua_register(lua2, "SendMessage_quitgame", SendMessage_quitgame);
	lua_register(lua2, "SendMessage_leavegame", SendMessage_leavegame);
	lua_register(lua2, "SendMessage_resumegame", SendMessage_resumegame);
	lua_register(lua2, "SendMessage_switchpageback", SendMessage_switchpageback);
	lua_register(lua2, "SendMessage_triggerfadein", SendMessage_triggerfadein);
	lua_register(lua2, "SendMessage_wingame", SendMessage_wingame);
	lua_register(lua2, "SendMessage_losegame", SendMessage_losegame);
	lua_register(lua2, "SendMessage_musicstop", SendMessage_musicstop);
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
	lua_register(lua2, "SendMessageI_collisionon", SendMessageI_collisionon);
	lua_register(lua2, "SendMessageI_drownplayer", SendMessageI_drownplayer);
	lua_register(lua2, "SendMessageI_finishlevel", SendMessageI_finishlevel);
	lua_register(lua2, "SendMessageI_hideterrain", SendMessageI_hideterrain);
	lua_register(lua2, "SendMessageI_promptvideo", SendMessageI_promptvideo);
	lua_register(lua2, "SendMessageI_promptimage", SendMessageI_promptimage);
	lua_register(lua2, "SendMessageI_showterrain", SendMessageI_showterrain);
	lua_register(lua2, "SendMessageI_spawnifused", SendMessageI_spawnifused);
	lua_register(lua2, "SendMessageI_textcenterx", SendMessageI_textcenterx);
	lua_register(lua2, "SendMessageI_setservertimer", SendMessageI_setservertimer);
	lua_register(lua2, "SendMessageI_setsoundvolume", SendMessageI_setsoundvolume);
	lua_register(lua2, "SendMessageI_unfreezeplayer", SendMessageI_unfreezeplayer);
	lua_register(lua2, "SendMessageI_activateifused", SendMessageI_activateifused);
	lua_register(lua2, "SendMessageI_addplayerpower", SendMessageI_addplayerpower);
	lua_register(lua2, "SendMessageI_loopnon3dsound", SendMessageI_loopnon3dsound);
	lua_register(lua2, "SendMessageI_musicsetlength", SendMessageI_musicsetlength);
	lua_register(lua2, "SendMessageI_musicsetvolume", SendMessageI_musicsetvolume);
	lua_register(lua2, "SendMessageI_playnon3dsound", SendMessageI_playnon3dsound);
	lua_register(lua2, "SendMessageI_rotatetocamera", SendMessageI_rotatetocamera);
	lua_register(lua2, "SendMessageI_rotatetoplayer", SendMessageI_rotatetoplayer);
	lua_register(lua2, "SendMessageI_prompttextsize", SendMessageI_prompttextsize);
	lua_register(lua2, "SendMessageI_setgamequality", SendMessageI_setgamequality);
	lua_register(lua2, "SendMessageI_setplayerhealthcore", SendMessageI_setplayerhealthcore);
	lua_register(lua2, "SendMessageI_setplayerlives", SendMessageI_setplayerlives);
	lua_register(lua2, "SendMessageI_setplayerpower", SendMessageI_setplayerpower);
	lua_register(lua2, "SendMessageI_hide", SendMessageI_hide);
	lua_register(lua2, "SendMessageI_show", SendMessageI_show);
	lua_register(lua2, "SendMessageI_hidelimbs", SendMessageI_hidelimbs);
	lua_register(lua2, "SendMessageI_showlimbs", SendMessageI_showlimbs);
	lua_register(lua2, "SendMessageI_setanimationframes", SendMessageI_setanimationframes);
	lua_register(lua2, "SendMessageI_removeplayerweapon", SendMessageI_removeplayerweapon);
	lua_register(lua2, "SendMessageI_setgamemusicvolume", SendMessageI_setgamemusicvolume);
	lua_register(lua2, "SendMessageI_setgamesoundvolume", SendMessageI_setgamesoundvolume);
	lua_register(lua2, "SendMessageI_setloadingresource", SendMessageI_setloadingresource);
	lua_register(lua2, "SendMessageI_getentityplrvisible", SendMessageI_getentityplrvisible);
	lua_register(lua2, "SendMessageI_replaceplayerweapon", SendMessageI_replaceplayerweapon);
	lua_register(lua2, "SendMessageI_setserverkillstowin", SendMessageI_setserverkillstowin);
	lua_register(lua2, "SendMessageI_removeplayerweapons", SendMessageI_removeplayerweapons);
	lua_register(lua2, "SendMessageI_setanimation", SendMessageI_setanimation);
	lua_register(lua2, "SendMessageI_setactivated", SendMessageI_setactivated);
	lua_register(lua2, "SendMessageI_collisionoff", SendMessageI_collisionoff);
	lua_register(lua2, "SendMessageI_freezeplayer", SendMessageI_freezeplayer);
	lua_register(lua2, "SendMessageI_musicplaycue", SendMessageI_musicplaycue);
	lua_register(lua2, "SendMessageI_nameplateson", SendMessageI_nameplateson);
	lua_register(lua2, "SendMessageI_resetlimbhit", SendMessageI_resetlimbhit);
	lua_register(lua2, "SendMessageI_ragdollforce", SendMessageI_ragdollforce);
	lua_register(lua2, "SendMessageI_setforcelimb", SendMessageI_setforcelimb);
	lua_register(lua2, "SendMessageI_setforcex", SendMessageI_setforcex);
	lua_register(lua2, "SendMessageI_setforcey", SendMessageI_setforcey);
	lua_register(lua2, "SendMessageI_setforcez", SendMessageI_setforcez);
	lua_register(lua2, "SendMessageI_setplayerfov", SendMessageI_setplayerfov);
	lua_register(lua2, "SendMessageI_setnogravity", SendMessageI_setnogravity);
	lua_register(lua2, "SendMessageI_setlimbindex", SendMessageI_setlimbindex);
	lua_register(lua2, "SendMessageI_destroy", SendMessageI_destroy);
	lua_register(lua2, "SendMessageI_dopanel", SendMessageI_dopanel);
	lua_register(lua2, "SendMessageI_textred", SendMessageI_textred);
	lua_register(lua2, "SendMessageI_freezeai", SendMessageI_freezeai);
	lua_register(lua2, "SendMessageI_hidehuds", SendMessageI_hidehuds);
	lua_register(lua2, "SendMessageI_setsound", SendMessageI_setsound);
	lua_register(lua2, "SendMessageI_showhuds", SendMessageI_showhuds);
	lua_register(lua2, "SendMessageI_textblue", SendMessageI_textblue);
	lua_register(lua2, "SendMessageI_textsize", SendMessageI_textsize);
	lua_register(lua2, "SendMessageI_collected", SendMessageI_collected);
	lua_register(lua2, "SendMessageI_hideimage", SendMessageI_hideimage);
	lua_register(lua2, "SendMessageI_hidewater", SendMessageI_hidewater);
	lua_register(lua2, "SendMessageI_loopsound", SendMessageI_loopsound);
	lua_register(lua2, "SendMessageI_playsound", SendMessageI_playsound);
	lua_register(lua2, "SendMessageI_playvideo", SendMessageI_playvideo);
	lua_register(lua2, "SendMessageI_stopvideo", SendMessageI_stopvideo);
	lua_register(lua2, "SendMessageI_showimage", SendMessageI_showimage);
	lua_register(lua2, "SendMessageI_showwater", SendMessageI_showwater);
	lua_register(lua2, "SendMessageI_stopsound", SendMessageI_stopsound);
	lua_register(lua2, "SendMessageI_textgreen", SendMessageI_textgreen);
	lua_register(lua2, "SendMessageI_checkpoint", SendMessageI_checkpoint);
	lua_register(lua2, "SendMessageI_fireweapon", SendMessageI_fireweapon);
	lua_register(lua2, "SendMessageI_hurtplayer", SendMessageI_hurtplayer);
	lua_register(lua2, "SendMessageI_mpgamemode", SendMessageI_mpgamemode);
	lua_register(lua2, "SendMessageI_playspeech", SendMessageI_playspeech);
	lua_register(lua2, "SendMessageI_stopspeech", SendMessageI_stopspeech);
	lua_register(lua2, "SendMessageI_starttimer", SendMessageI_starttimer);
	lua_register(lua2, "SendMessageI_unfreezeai", SendMessageI_unfreezeai);
	lua_register(lua2, "SendMessageI_disablemusicreset", SendMessageI_disablemusicreset);
	lua_register(lua2, "SendMessageI_fireweaponinstant", SendMessageI_fireweaponinstant);
	lua_register(lua2, "SendMessageI_loopanimationfrom", SendMessageI_loopanimationfrom);
	lua_register(lua2, "SendMessageI_movewithanimation", SendMessageI_movewithanimation);
	lua_register(lua2, "SendMessageI_playanimationfrom", SendMessageI_playanimationfrom);
	lua_register(lua2, "SendMessageI_setactivatedformp", SendMessageI_setactivatedformp);
	lua_register(lua2, "SendMessageI_promptvideonoskip", SendMessageI_promptvideonoskip);
	lua_register(lua2, "SendMessageI_playsoundifsilent", SendMessageI_playsoundifsilent);
	lua_register(lua2, "SendMessageI_setimagealignment", SendMessageI_setimagealignment);
	lua_register(lua2, "SendMessageI_transporttoifused", SendMessageI_transporttoifused);
	lua_register(lua2, "SendMessageI_serverendplay", SendMessageI_serverendplay);
	lua_register(lua2, "SendMessageI_activatemouse", SendMessageI_activatemouse);
	lua_register(lua2, "SendMessageI_addplayerammo", SendMessageI_addplayerammo);
	lua_register(lua2, "SendMessageI_loopanimation", SendMessageI_loopanimation);
	lua_register(lua2, "SendMessageI_musicplaytime", SendMessageI_musicplaytime);
	lua_register(lua2, "SendMessageI_musicplayfade", SendMessageI_musicplayfade);
	lua_register(lua2, "SendMessageI_playanimation", SendMessageI_playanimation);
	lua_register(lua2, "SendMessageI_nameplatesoff", SendMessageI_nameplatesoff);
	lua_register(lua2, "SendMessageI_refreshentity", SendMessageI_refreshentity);
	lua_register(lua2, "SendMessageI_setsoundspeed", SendMessageI_setsoundspeed);
	lua_register(lua2, "SendMessageI_stopanimation", SendMessageI_stopanimation);
	lua_register(lua2, "SendMessageI_deactivatemouse", SendMessageI_deactivatemouse);
	lua_register(lua2, "SendMessageI_addplayerhealth", SendMessageI_addplayerhealth);
	lua_register(lua2, "SendMessageI_addplayerweapon", SendMessageI_addplayerweapon);
	lua_register(lua2, "SendMessageI_getentityinzone", SendMessageI_getentityinzone);
	lua_register(lua2, "SendMessageI_musicsetdefault", SendMessageI_musicsetdefault);
	lua_register(lua2, "SendMessageI_playvideonoskip", SendMessageI_playvideonoskip);
	lua_register(lua2, "SendMessageI_setentityhealth", SendMessageI_setentityhealth);
	lua_register(lua2, "SendMessageI_setlightvisible", SendMessageI_setlightvisible);
	lua_register(lua2, "SendMessageI_addplayerjetpack", SendMessageI_addplayerjetpack);
	lua_register(lua2, "SendMessageI_musicplayinstant", SendMessageI_musicplayinstant);
	lua_register(lua2, "SendMessageI_musicplaytimecue", SendMessageI_musicplaytimecue);
	lua_register(lua2, "SendMessageI_musicsetfadetime", SendMessageI_musicsetfadetime);
	lua_register(lua2, "SendMessageI_musicsetinterval", SendMessageI_musicsetinterval);
	lua_register(lua2, "SendMessageI_serverrespawnall", SendMessageI_serverrespawnall);
	lua_register(lua2, "SendMessageI_setlockcharacter", SendMessageI_setlockcharacter);
	lua_register(lua2, "SendMessageI_spawn", SendMessageI_spawn);
	lua_register(lua2, "SendMessageI_changeplayerweaponid", SendMessageI_changeplayerweaponid);
	lua_register(lua2, "SendMessageI_setcharactersoundset", SendMessageI_setcharactersoundset);
	lua_register(lua2, "SendMessageI_setcharactertostrafe", SendMessageI_setcharactertostrafe);
	lua_register(lua2, "SendMessageI_charactercontrolarmed", SendMessageI_charactercontrolarmed);
	lua_register(lua2, "SendMessageI_charactercontrollimbo", SendMessageI_charactercontrollimbo);
	lua_register(lua2, "SendMessageI_charactercontrolstand", SendMessageI_charactercontrolstand);
	lua_register(lua2, "SendMessageI_setcharactertowalkrun", SendMessageI_setcharactertowalkrun);
	lua_register(lua2, "SendMessageI_setentityhealthsilent", SendMessageI_setentityhealthsilent);
	lua_register(lua2, "SendMessageI_charactercontrolducked", SendMessageI_charactercontrolducked);
	lua_register(lua2, "SendMessageI_charactercontrolfidget", SendMessageI_charactercontrolfidget);
	lua_register(lua2, "SendMessageI_charactercontrolmanual", SendMessageI_charactercontrolmanual);
	lua_register(lua2, "SendMessageI_charactercontrolunarmed", SendMessageI_charactercontrolunarmed);
	lua_register(lua2, "SendMessageI_performlogicconnections", SendMessageI_performlogicconnections);
	lua_register(lua2, "SendMessageI_performlogicconnectionnumber", SendMessageI_performlogicconnectionnumber);
	lua_register(lua2, "SendMessageI_setcharactervisiondelay", SendMessageI_setcharactervisiondelay);
	lua_register(lua2, "SendMessageI_transporttofreezeposition", SendMessageI_transporttofreezeposition);
	lua_register(lua2, "SendMessageI_setentityhealthwithdamage", SendMessageI_setentityhealthwithdamage);
	lua_register(lua2, "SendMessageI_performlogicconnectionsaskey", SendMessageI_performlogicconnectionsaskey);
	lua_register(lua2, "SendMessageI_setprioritytotransporter", SendMessageI_setprioritytotransporter);
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
	lua_register(lua2, "SendMessageF_lookatangle", SendMessageF_lookatangle);
	lua_register(lua2, "SendMessageF_lookforward", SendMessageF_lookforward);
	lua_register(lua2, "SendMessageF_moveforward", SendMessageF_moveforward);
	lua_register(lua2, "SendMessageF_rotatelimbx", SendMessageF_rotatelimbx);
	lua_register(lua2, "SendMessageF_rotatelimby", SendMessageF_rotatelimby);
	lua_register(lua2, "SendMessageF_rotatelimbz", SendMessageF_rotatelimbz);
	lua_register(lua2, "SendMessageF_setfoggreen", SendMessageF_setfoggreen);
	lua_register(lua2, "SendMessageF_texty", SendMessageF_texty);
	lua_register(lua2, "SendMessageF_setsurfaceblue", SendMessageF_setsurfaceblue);
	lua_register(lua2, "SendMessageF_setterrainsize", SendMessageF_setterrainsize);
	lua_register(lua2, "SendMessageF_setambiencered", SendMessageF_setambiencered);
	lua_register(lua2, "SendMessageF_resetpositionx", SendMessageF_resetpositionx);
	lua_register(lua2, "SendMessageF_resetpositiony", SendMessageF_resetpositiony);
	lua_register(lua2, "SendMessageF_resetpositionz", SendMessageF_resetpositionz);
	lua_register(lua2, "SendMessageF_resetrotationx", SendMessageF_resetrotationx);
	lua_register(lua2, "SendMessageF_resetrotationy", SendMessageF_resetrotationy);
	lua_register(lua2, "SendMessageF_resetrotationz", SendMessageF_resetrotationz);
	lua_register(lua2, "SendMessageF_setfogdistance", SendMessageF_setfogdistance);
	lua_register(lua2, "SendMessageF_sethoverfactor", SendMessageF_sethoverfactor);
	lua_register(lua2, "SendMessageF_changeplayerweapon", SendMessageF_changeplayerweapon);
	lua_register(lua2, "SendMessageF_playcharactersound", SendMessageF_playcharactersound);
	lua_register(lua2, "SendMessageF_setcameraweaponfov", SendMessageF_setcameraweaponfov);
	lua_register(lua2, "SendMessageF_setanimationframes", SendMessageF_setanimationframes);
	lua_register(lua2, "SendMessageF_setfreezepositionx", SendMessageF_setfreezepositionx);
	lua_register(lua2, "SendMessageF_setfreezepositiony", SendMessageF_setfreezepositiony);
	lua_register(lua2, "SendMessageF_setfreezepositionz", SendMessageF_setfreezepositionz);
	lua_register(lua2, "SendMessageF_setgamemusicvolume", SendMessageF_setgamemusicvolume);
	lua_register(lua2, "SendMessageF_setgamesoundvolume", SendMessageF_setgamesoundvolume);
	lua_register(lua2, "SendMessageF_setloadingresource", SendMessageF_setloadingresource);
	lua_register(lua2, "SendMessageF_setoptionlightrays", SendMessageF_setoptionlightrays);
	lua_register(lua2, "SendMessageF_setoptionocclusion", SendMessageF_setoptionocclusion);
	lua_register(lua2, "SendMessageF_setvegetationwidth", SendMessageF_setvegetationwidth);
	lua_register(lua2, "SendMessageF_levelfilenametoload", SendMessageF_levelfilenametoload);
	lua_register(lua2, "SendMessageF_replaceplayerweapon", SendMessageF_replaceplayerweapon);
	lua_register(lua2, "SendMessageF_setfreezepositionax", SendMessageF_setfreezepositionax);
	lua_register(lua2, "SendMessageF_setfreezepositionay", SendMessageF_setfreezepositionay);
	lua_register(lua2, "SendMessageF_setfreezepositionaz", SendMessageF_setfreezepositionaz);
	lua_register(lua2, "SendMessageF_setoptionreflection", SendMessageF_setoptionreflection);
	lua_register(lua2, "SendMessageF_setoptionvegetation", SendMessageF_setoptionvegetation);
	lua_register(lua2, "SendMessageF_setpostsaointensity", SendMessageF_setpostsaointensity);
	lua_register(lua2, "SendMessageF_setsurfaceintensity", SendMessageF_setsurfaceintensity);
	lua_register(lua2, "SendMessageF_setsurfacesunfactor", SendMessageF_setsurfacesunfactor);
	lua_register(lua2, "SendMessageF_setvegetationheight", SendMessageF_setvegetationheight);
	lua_register(lua2, "SendMessageF_stopparticleemitter", SendMessageF_stopparticleemitter);
	lua_register(lua2, "SendMessageF_lookattargetyoffset", SendMessageF_lookattargetyoffset);
	lua_register(lua2, "SendMessageF_setconstrast", SendMessageF_setconstrast);
	lua_register(lua2, "SendMessageF_setcamerafov", SendMessageF_setcamerafov);
	lua_register(lua2, "SendMessageF_lookattarget", SendMessageF_lookattarget);
	lua_register(lua2, "SendMessageF_lookatplayer", SendMessageF_lookatplayer);
	lua_register(lua2, "SendMessageF_movebackward", SendMessageF_movebackward);
	lua_register(lua2, "SendMessageF_mp_aimovetox", SendMessageF_mp_aimovetox);
	lua_register(lua2, "SendMessageF_mp_aimovetoz", SendMessageF_mp_aimovetoz);
	lua_register(lua2, "SendMessageF_setrotationx", SendMessageF_setrotationx);
	lua_register(lua2, "SendMessageF_setrotationy", SendMessageF_setrotationy);
	lua_register(lua2, "SendMessageF_setrotationz", SendMessageF_setrotationz);
	lua_register(lua2, "SendMessageF_setpositionx", SendMessageF_setpositionx);
	lua_register(lua2, "SendMessageF_setpositiony", SendMessageF_setpositiony);
	lua_register(lua2, "SendMessageF_setpositionz", SendMessageF_setpositionz);
	lua_register(lua2, "SendMessageF_setpostbloom", SendMessageF_setpostbloom);
	lua_register(lua2, "SendMessageF_moveup", SendMessageF_moveup);
	lua_register(lua2, "SendMessageF_panelx", SendMessageF_panelx);
	lua_register(lua2, "SendMessageF_panely", SendMessageF_panely);
	lua_register(lua2, "SendMessageF_panelx2", SendMessageF_panelx2);
	lua_register(lua2, "SendMessageF_panely2", SendMessageF_panely2);
	lua_register(lua2, "SendMessageF_rotatex", SendMessageF_rotatex);
	lua_register(lua2, "SendMessageF_rotatey", SendMessageF_rotatey);
	lua_register(lua2, "SendMessageF_rotatez", SendMessageF_rotatez);
	lua_register(lua2, "SendMessageF_setfogred", SendMessageF_setfogred);
	lua_register(lua2, "SendMessageF_setfogblue", SendMessageF_setfogblue);
	lua_register(lua2, "SendMessageF_setcameradistance", SendMessageF_setcameradistance);
	lua_register(lua2, "SendMessageF_setanimationframe", SendMessageF_setanimationframe);
	lua_register(lua2, "SendMessageF_changeanimationframe", SendMessageF_changeanimationframe);
	lua_register(lua2, "SendMessageF_setanimationspeed", SendMessageF_setanimationspeed);
	lua_register(lua2, "SendMessageF_setglobalspecular", SendMessageF_setglobalspecular);
	lua_register(lua2, "SendMessageF_setimagepositionx", SendMessageF_setimagepositionx);
	lua_register(lua2, "SendMessageF_setimagepositiony", SendMessageF_setimagepositiony);
	lua_register(lua2, "SendMessageF_setterrainlodnear", SendMessageF_setterrainlodnear);
	lua_register(lua2, "SendMessageF_setbrightness", SendMessageF_setbrightness);
	lua_register(lua2, "SendMessageF_aimsmoothmode", SendMessageF_aimsmoothmode);
	lua_register(lua2, "SendMessageF_lookattargete", SendMessageF_lookattargete);
	lua_register(lua2, "SendMessageF_modulatespeed", SendMessageF_modulatespeed);
	lua_register(lua2, "SendMessageF_setfognearest", SendMessageF_setfognearest);
	lua_register(lua2, "SendMessageF_setsurfacered", SendMessageF_setsurfacered);
	lua_register(lua2, "SendMessageF_setambienceblue", SendMessageF_setambienceblue);
	lua_register(lua2, "SendMessageF_setfogintensity", SendMessageF_setfogintensity);
	lua_register(lua2, "SendMessageF_setsurfacegreen", SendMessageF_setsurfacegreen);
	lua_register(lua2, "SendMessageF_setambiencegreen", SendMessageF_setambiencegreen);
	lua_register(lua2, "SendMessageF_setoptionshadows", SendMessageF_setoptionshadows);
	lua_register(lua2, "SendMessageF_setpostsaoradius", SendMessageF_setpostsaoradius);
	lua_register(lua2, "SendMessageF_setterrainlodfar", SendMessageF_setterrainlodfar);
	lua_register(lua2, "SendMessageF_setterrainlodmid", SendMessageF_setterrainlodmid);
	lua_register(lua2, "SendMessageF_scale", SendMessageF_scale);
	lua_register(lua2, "SendMessageF_textx", SendMessageF_textx);
	lua_register(lua2, "SendMessageF_promptlocalforvrmode", SendMessageF_promptlocalforvrmode);
	lua_register(lua2, "SendMessageF_setambienceintensity", SendMessageF_setambienceintensity);
	lua_register(lua2, "SendMessageF_setpostlightraydecay", SendMessageF_setpostlightraydecay);
	lua_register(lua2, "SendMessageF_startparticleemitter", SendMessageF_startparticleemitter);
	lua_register(lua2, "SendMessageF_setpostlightraylength", SendMessageF_setpostlightraylength);
	lua_register(lua2, "SendMessageF_setpostmotiondistance", SendMessageF_setpostmotiondistance);
	lua_register(lua2, "SendMessageF_setpostvignetteradius", SendMessageF_setpostvignetteradius);
	lua_register(lua2, "SendMessageF_setvegetationquantity", SendMessageF_setvegetationquantity);
	lua_register(lua2, "SendMessageF_setpostmotionintensity", SendMessageF_setpostmotionintensity);
	lua_register(lua2, "SendMessageF_setpostlightrayquality", SendMessageF_setpostlightrayquality);
	lua_register(lua2, "SendMessageF_setcamerazoompercentage", SendMessageF_setcamerazoompercentage);
	lua_register(lua2, "SendMessageF_rotatetoplayerwithoffset", SendMessageF_rotatetoplayerwithoffset);
	lua_register(lua2, "SendMessageF_setpostvignetteintensity", SendMessageF_setpostvignetteintensity);
	lua_register(lua2, "SendMessageF_setpostlensflareintensity", SendMessageF_setpostlensflareintensity);
	lua_register(lua2, "SendMessageF_setpostdepthoffielddistance", SendMessageF_setpostdepthoffielddistance);
	lua_register(lua2, "SendMessageF_setpostdepthoffieldintensity", SendMessageF_setpostdepthoffieldintensity);
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
	lua_register(lua2, "SendMessageS_jumptolevel", SendMessageS_jumptolevel);
	lua_register(lua2, "SendMessageS_promptlocal", SendMessageS_promptlocal);
	lua_register(lua2, "SendMessageS_playcharactersound", SendMessageS_playcharactersound);
	lua_register(lua2, "SendMessageS_promptduration", SendMessageS_promptduration);
	lua_register(lua2, "SendMessageS_changeplayerweapon", SendMessageS_changeplayerweapon);
	lua_register(lua2, "SendMessageS_levelfilenametoload", SendMessageS_levelfilenametoload);
	lua_register(lua2, "SendMessageS_switchscript", SendMessageS_switchscript);
	lua_register(lua2, "SendMessageS_prompt", SendMessageS_prompt);
	lua_register(lua2, "SendMessageS_texttxt", SendMessageS_texttxt);
	lua_register(lua2, "SendMessageS_setskyto", SendMessageS_setskyto);
	lua_register(lua2, "SendMessageS_musicload", SendMessageS_musicload);
	lua_register(lua2, "SendMessageS_switchpage", SendMessageS_switchpage);
	lua_register(lua2, "SendMessageS_setanimationname", SendMessageS_setanimationname);
	lua_register(lua2, "SendMessageS_promptlocalforvr", SendMessageS_promptlocalforvr);
	lua_register(lua2, "SendMessageS_loadimages", SendMessageS_loadimages);
	lua_register(lua2, "SendMessageS_setcharactersound", SendMessageS_setcharactersound);
}

