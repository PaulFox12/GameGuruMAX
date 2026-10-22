 int GetEntityData ( lua_State *L, int iDataMode )
 {
	lua2 = L;
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
			lua_pushinteger ( L, (lua_Integer)iReturnValue );
		}
	}
	else
	{
		lua_pushinteger ( L, 0 );
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
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 const char* pString = lua_tostring(L, 2);
	 t.entityelement[iEntityIndex].eleprof.ifused_s = pString;
	 return 0;
 }
 int GetEntityIfUsed(lua_State* L)
 {
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iNewValue = lua_tonumber(L, 2);
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1 || n > 2) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iSubscriptValue = lua_tonumber(L, 2);
	if (iEntityIndex > 0)
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
		lua_pushinteger (L, 0);
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
	 lua2 = L;
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
	 lua_pushinteger (L, (lua_Integer)iCount);
	 return 1;
 }
 int GetEntityWithinCone(lua_State *L)
 {
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iIndex = lua_tonumber(L, 1);
	 int iEntityIndex = 0;
	 if (iIndex >= 0 && iIndex < g_entitiesinconelist.size())
	 {
		 iEntityIndex = g_entitiesinconelist[iIndex];
	 }
	 // return entity e from list at this index
	 lua_pushinteger (L, (lua_Integer)iEntityIndex);
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
	 lua_pushinteger (L, (lua_Integer)iBestE);
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
	 lua_pushinteger (L, (lua_Integer)iWhoE);
	 return 2;
 }
 int MakeAISound (lua_State *L)
 {
	 lua2 = L;
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
	lua2 = L;
	int n = lua_gettop( L );
	if ( n < 3 ) return 0;
	bool setSound = ( n == 4 ) && lua_tonumber( L, 4 ) == 1;
	int iReturnValue = 0;
	int iEntityIndex    = lua_tonumber( L, 1 );
	int iSlotIndex      = lua_tonumber( L, 2 );
	const char* pString = lua_tostring( L, 3 );
	if ( iEntityIndex > 0 )
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iSlotIndex = lua_tonumber(L, 2);
	LPSTR pString = "";
	if ( iEntityIndex > 0 )
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
	 lua2 = L;
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

 // Entity Animation
 int SetEntityAnimation(lua_State *L)
 {
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iAnimationSetIndex = lua_tonumber(L, 2);
	int iAnimationSetStart = lua_tonumber(L, 3);
	int iAnimationSetFinish = lua_tonumber(L, 4);
	if ( iEntityIndex > 0 )
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iAnimationSetIndex = lua_tonumber(L, 2);
	if ( iEntityIndex > 0 )
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iAnimationSetIndex = lua_tonumber(L, 2);
	if ( iEntityIndex > 0 )
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iAnimationSetIndex = lua_tonumber(L, 2);
	if ( iEntityIndex > 0 )
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
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1 || n > 2) return 0;
	 int iReturnValue = 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 int iChopFramesOffThEnd = lua_tonumber(L, 2);
	 if (iEntityIndex > 0)
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
	 lua2 = L;
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if ( iEntityIndex > 0 )
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	int iReturnValue = 0;
	int iEntityIndex = lua_tonumber(L, 1);
	int iFootfallIndex = lua_tonumber(L, 2);
	int iLeftOrRight = lua_tonumber(L, 3);
	if ( iEntityIndex > 0 )
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
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iReturnValue = 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 const char* pAnimationName = lua_tostring(L, 2);
	 if (iEntityIndex > 0)
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
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 2) return 0;
	 int iReturnValue = -1;
	 int iEntityIndex = lua_tonumber(L, 1);
	 int iTriggerIndex = lua_tonumber(L, 2);
	 if (iEntityIndex > 0)
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
	 lua2 = L;
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
	 lua2 = L;
	 int iEntityCount = g.entityelementlist;
	 lua_pushinteger (L, iEntityCount);
	 return 1;
 }

 int CreateEntityIfNotPresent(lua_State* L)
 {
	 lua2 = L;
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
	 return 0;
 }

 // the last clone's time and steps (SpawnProbe_Stop), for SpawnNewEntity's log line (the DX11 branch's spawn probe, a0596f3b)
 static char g_pSpawnProbeText[1024] = "";

 int SpawnNewEntityCore(int iEntityIndex)
 {
	#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
	#endif
	int iNewE = -1;
	SpawnProbe_Start();
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
	SpawnProbe_Stop(g_pSpawnProbeText, sizeof(g_pSpawnProbeText));
	return iNewE;
 }
 
 std::vector<int> vSpawnList;

 int SpawnNewEntity(lua_State* L)
 {
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iNewE = -1;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (iEntityIndex > 0)
	 {
		 iNewE = SpawnNewEntityCore(iEntityIndex);
		 vSpawnList.push_back(iNewE);
		 // with the clone's model and the time of each step (a diagnostic)
		 char pMsg[1536];
		 int iBank = t.entityelement[iEntityIndex].bankindex;
		 sprintf_s(pMsg, "SpawnNewEntityCore : %d from %d, %s in %s", iNewE, iEntityIndex, iBank > 0 ? t.entitybank_s[iBank].Get() : "?", g_pSpawnProbeText);
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
		 // GG: or a clone spawned into a free element inside the level's list (entity_addentitytomap reuses the first free one,
		 // which the index test missed, so the delete did nothing)
		 if (iEntityIndex > 0 && iEntityIndex < t.entityelement.size() && (iEntityIndex > t.storedentityelementlist || t.entityelement[iEntityIndex].iWasSpawnedInGame > 0))
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
				 // GG: its ragdoll goes with it, before the object: left behind, its 13 or so bodies fell for ever, awake
				 extern int BPhys_RagdollExist(int ragdollID);
				 if (BPhys_RagdollExist(t.entityelement[t.tentitytoselect].obj) == 1)
				 {
					 t.tphyobj = t.entityelement[t.tentitytoselect].obj; ragdoll_destroy ();
				 }
				 // the frame's own scene update applies the removal: a full Scene::Update for each deleted object cost about 5 ms on Outdoors (as the gun's deletes already skip it)
				 extern bool bBlockSceneUpdate;
				 bBlockSceneUpdate = true;
				 entity_deleteentityfrommap();
				 bBlockSceneUpdate = false;
				 t.entityelement[t.tentitytoselect].iWasSpawnedInGame = 0;
				 if (t.entityelement[t.tentitytoselect].ragdollified == 1)
				 {
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
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 1) return 0;
	 int iEntityIndex = lua_tonumber(L, 1);
	 if (iEntityIndex > 0)
	 {
		 // GG: or a clone spawned into a free element inside the level's list (entity_addentitytomap reuses the first free one,
		 // which the index test missed, so the delete did nothing and the clone lived on)
		 if (iEntityIndex > t.storedentityelementlist || (iEntityIndex < t.entityelement.size() && t.entityelement[iEntityIndex].iWasSpawnedInGame > 0))
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
			 // GG: its ragdoll goes with it, before the object: left behind, its 13 or so bodies fell for ever, awake
			 extern int BPhys_RagdollExist(int ragdollID);
			 if (BPhys_RagdollExist(t.entityelement[t.tentitytoselect].obj) == 1)
			 {
				 t.tphyobj = t.entityelement[t.tentitytoselect].obj; ragdoll_destroy ();
			 }
			 // the frame's own scene update applies the removal: a full Scene::Update for each deleted object cost about 5 ms on Outdoors (as the gun's deletes already skip it)
			 extern bool bBlockSceneUpdate;
			 bBlockSceneUpdate = true;
			 entity_deleteentityfrommap ();
			 bBlockSceneUpdate = false;
			 t.entityelement[t.tentitytoselect].iWasSpawnedInGame = 0;
			 if (t.entityelement[t.tentitytoselect].ragdollified == 1)
			 {
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
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
	lua2 = L;
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
	 lua2 = L;
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
	 if (::Timer() > iUpdateTimer + 55) //PE: 1000/20 = 50 (20 fps).
	 {
		 iUpdateTimer = ::MAXTimer();
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
	 lua2 = L;
	 int n = LUA_GETTOP(L);
	 if (n < 3) return 0;
	 float fReturnHeight = 0.0f;
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
	 }
	 #else
	 fReturnHeight = GetLUATerrainHeightEx(fX, fZ);
	 #endif
	 lua_pushinteger (L, fReturnHeight);
	 return 1;
 }

 // DarkAI
 int AISetEntityControl(lua_State *L)
 {
	 lua2 = L;

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
	 lua2 = L;

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
	 lua2 = L;

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
	 lua2 = L;

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
	lua2 = L;

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
	lua2 = L;

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
	lua2 = L;

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
	lua2 = L;

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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;
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
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	if ( n < 1 ) return 0;

	MessageBox(NULL, lua_tostring(L, 1), "LUA MESSAGE", MB_TOPMOST | MB_OK);

	return 0;
}

int AISetEntityMoveBoostPriority(lua_State *L)
{
	lua2 = L;
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	int iObj = lua_tointeger(L, 1);
	float fGoToX = lua_tonumber(L, 2);
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
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
	lua_pushinteger (L, 0);
#else
	lua_pushinteger ( L , (lua_Integer)AIGetEntityHeardSound ( lua_tonumber(L, 1) ) );
#endif
	return 1;
}

int AISetData ( lua_State *L, int iDataMode )
{
	lua2 = L;
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
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
#ifdef WICKEDENGINE
	// No subsystem for AI in MAX
	lua_pushinteger (L, 0);
#else
	switch ( iDataMode )
	{
		case 1 : lua_pushnumber ( L , AIGetEntityAngleY ( lua_tonumber(L, 1) ) ); break;
		case 2 : if ( t.aisystem.processlogic == 0 )
					lua_pushinteger ( L , 0 );
				 else
					lua_pushinteger ( L , (lua_Integer)AIGetEntityIsMoving ( lua_tonumber(L, 1) ) );
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
	lua2 = L;
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
		default : lua_pushinteger ( L, 0 ); break;
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
	lua2 = L;

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

