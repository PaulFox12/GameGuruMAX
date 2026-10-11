//----------------------------------------------------
//--- GAMEGURU - M-Weapon
//----------------------------------------------------

#include "stdafx.h"
#include "gameguru.h"
#include "GGVR.h"

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

void weapon_preloadfiles ( void )
{
	//PE: Not used in wicked.
}

void weapon_getfreeobject ( void )
{
	// sets tObjID to the next available object ID, or 0 if none available
	for ( t.obj = g.weaponsobjectoffset; t.obj <= g.weaponsobjectoffset + 499; t.obj++ )
	{
		if (  ObjectExist(t.obj)  ==  0 ) 
		{
			t.tObjID = t.obj ; return;
		}
	}
	t.tObjID = 0;
}

void weapon_getfreeimage ( void )
{
	// sets tImgID to the next available image ID, or 0 if none available
	for ( t.timg = g.weaponsimageoffset ; t.timg <=  g.weaponsimageoffset + 299; t.timg++ )
	{
		if (  ImageExist(t.timg)  ==  0 ) 
		{
			t.tImgID = t.timg ; return;
		}
	}
	t.tImgID = 0;
}

void weapon_getfreesound ( void )
{
	// sets tSndID to the next available sound ID, or 0 if none available
	for ( t.tsnd = g.projectilesoundoffset ; t.tsnd <=  g.projectilesoundoffset + 299; t.tsnd++ )
	{
		if (  SoundExist(t.tsnd)  ==  0 ) 
		{
			t.tSndID = t.tsnd ; return;
		}
	}
	t.tSndID = 0;
}

void weapon_loadobject ( void )
{
	// loads in an object tFileName$, returns tObjID (0 if failed, or DBP ID of object)
	weapon_getfreeobject ( );
	if (  t.tObjID  ==  0  )  return;
	char pChopFile[512];
	strcpy ( pChopFile, t.tFileName_s.Get() );
	pChopFile[strlen(pChopFile)-2]=0;
	cstr pFileToLoad = cstr(pChopFile) + cstr(".DBO");
	if ( FileExist ( pFileToLoad.Get() ) == 0 ) 
	{
		pFileToLoad = t.tFileName_s;
		if (FileExist (pFileToLoad.Get()) == 1)
		{
			LoadObject (pFileToLoad.Get(), t.tObjID);
		}
		else
		{
			// avoid attempting to load non existent projectile model (for ones that HAVE NO MODEL)
		}
	}
	else
	{
		LoadObject ( pFileToLoad.Get(), t.tObjID );
		// we rely on directly loading DBOs for the weapon system
		if (ObjectExist(t.tObjID))
			SetObjectDiffuseEx(t.tObjID, 0xFFFFFFFF, 0);
	}
	if (ObjectExist(t.tObjID) == 0)
	{
		// AssImp may fail to load some old X file styles, so have a fallback
		MakeObjectCube(t.tObjID, 0);// 25); and invisible infinitely tiny box :)
	}
	SetObjectTransparency ( t.tObjID, 6 );
	SetObjectEffect ( t.tObjID, g.guishadereffectindex );
	if (  ObjectExist(t.tObjID)  ==  0 ) 
	{
		t.tObjID = 0; return;
	}
}

void weapon_loadtexture ( void )
{
	// loads in a texture tFileName$, return tImgID (0 if failed, or DBP ID of image)
	if ( FileExist(t.tFileName_s.Get())  ==  0 ) 
	{
		t.tImgID = 0; return;
	}
	weapon_getfreeimage ( );
	if ( t.tImgID  ==  0  )  return;

	// and we have possible  access to the loaded textures via thread loader
	LoadImage ( t.tFileName_s.Get(), t.tImgID );

	if ( ImageExist(t.tImgID)  ==  0 ) 
	{
		t.tImgID = 0; return;
	}
}

void weapon_loadtextfile ( void )
{
	//  loads in a Text ( file ready to be used with _weapon_readfield. Must UnDim (  the fileData$ array after use ) )
	//  tTextFile$, returns tResult 0 (not found) or 1 loaded
	if (  FileExist(t.tTextFile_s.Get()) == 0 ) 
	{
		t.tResult = 0 ; return;
	}
	Dim (  t.fileData_s,300  );
	LoadArray (  t.tTextFile_s.Get(),t.fileData_s );
	t.tResult = 1;
}

void weapon_readfield ( void )
{
	//  reads the file loaded by _weapon_loadtextfile and reads the field specified by tInField$
	//  takes tInField$, populates tResult, value$, value1#, value2#
	t.tInField_s = Lower(t.tInField_s.Get());
	for ( t.l = 0 ; t.l<=  299; t.l++ )
	{
		t.line_s=t.fileData_s[t.l];
		if (  Len(t.line_s.Get())>0 ) 
		{
			if (  t.line_s.Get()[0] != ';' )
			{

				//  take fieldname and value
				t.mid = 0;
				for ( t.c = 0 ; t.c < Len(t.line_s.Get()); t.c++ )
				{
					if (  t.line_s.Get()[t.c] == '=' ) { t.mid = t.c+1  ; break; }
				}

				//  make sure we found the = character before continuing
				if (  t.mid > 0 ) 
				{
					t.field_s=Lower(removeedgespaces(Left(t.line_s.Get(),t.mid-1)));
					if (  t.field_s  ==  t.tInField_s ) 
					{
						t.value_s=removeedgespaces(Right(t.line_s.Get(),Len(t.line_s.Get())-t.mid));

						//  take value 1 and 2 from value$
						for ( t.c = 0 ; t.c < Len(t.value_s.Get()); t.c++ )
						{
							if (  t.value_s.Get()[t.c] == ',' ) { t.mid = t.c+1  ; break; }
						}
						t.value1_f=ValF(removeedgespaces(Left(t.value_s.Get(),t.mid-1)));
						t.value2_f=ValF(removeedgespaces(Right(t.value_s.Get(),Len(t.value_s.Get())-t.mid)));
						t.tResult = 1 ; return;
					}
				}
			}
		}
	}
	t.value_s = "";
	t.value1_f = 0;
	t.value2_f = 0;
	t.tResult = 0;
}

void weapon_init ( void )
{
}

void weapon_free ( void )
{
}

void weapon_loop ( void )
{
}

void weapon_load ( void )
{
}

void weapon_setstate ( void )
{
}

void weapon_addanimation ( void )
{
	t.WeaponAnimation[t.tWeapon][t.tAnim].startFrame = t.tStartFrame;
	t.WeaponAnimation[t.tWeapon][t.tAnim].endFrame = t.tEndFrame;
	t.WeaponAnimation[t.tWeapon][t.tAnim].speed = t.tSpeed;
	t.WeaponAnimation[t.tWeapon][t.tAnim].loopFlag = t.tLoopFlag;
}

void weapon_processanimation ( void )
{
}

// projectile code

void weapon_projectile_init ( void )
{

	// scan all projectiles in 'projectiletypes' folder, in the install folder and then the writable folder (as the guns
	// are, gun_scaninall_ref), so a projectile type added to the user's gamecore is found. A name found in both loads
	// once (weapon_projectile_load returns the existing one), and its files are read from the writable copy if present
	timestampactivity (0, "Preparing ProjectileTypes");
	cstr pRootDir = GetDir();
	for (int iProjectilesInTwoFolders = 0; iProjectilesInTwoFolders < 2; iProjectilesInTwoFolders++)
	{
		if (iProjectilesInTwoFolders == 1)
		{
			// writable folder - switch to it
			char pathToUse[MAX_PATH];
			strcpy(pathToUse, pRootDir.Get());
			GG_GetRealPath(pathToUse, 0);
			SetDir(pathToUse);
		}
		UnDim ( t.filelist_s );
		if ( PathExist("gamecore") == 1 )
		{
			SetDir ( "gamecore" );
			buildfilelist("projectiletypes","");
			SetDir ( ".." );
		}

		// the loader reads its files relative to the root folder
		SetDir(pRootDir.Get());
		if ( ArrayCount(t.filelist_s) > 0 )
		{
			timestampactivity (0, "Loading ProjectileTypes");
			for ( t.chkfile = 0 ; t.chkfile <= ArrayCount(t.filelist_s); t.chkfile++ )
			{
				t.file_s = t.filelist_s[t.chkfile];
				timestampactivity (0, t.file_s.Get());
				if ( t.file_s != "." && t.file_s != ".." )
				{
					if ( cstr(Lower(Right(t.file_s.Get(),4))) == ".txt" )
					{
						// get folder name only
						char pPathOnly[1024];
						strcpy ( pPathOnly, t.file_s.Get() );
						for ( int n = strlen(pPathOnly); n > 0; n-- )
						{
							if ( pPathOnly[n] == '\\' || pPathOnly[n] == '/' )
							{
								pPathOnly[n] = 0;
								break;
							}
						}
						t.tProjectileName_s = pPathOnly;
						timestampactivity (0, t.tProjectileName_s.Get());
						weapon_projectile_load ( );
					}
				}
			}
		}
	}
}

void weapon_getprojectileid ( void )
{
	t.tProjectileType = 0;
	for ( t.tProj = 1 ; t.tProj <= g.weaponSystem.numProjectiles; t.tProj++ )
	{
		t.tProjType = t.WeaponProjectile[t.tProj].baseType;
		if ( cstr(Lower(t.WeaponProjectileBase[t.tProjType].name_s.Get())) == t.tProjectileType_s ) 
		{
			//We need this projectile , setup cache.
			if (t.WeaponProjectileBase[t.tProjType].cacheLoaded == false) {
				int iSndForBaseSound = 0;
				int iSndForBaseDestroy = 0;
				t.tNewProjBase = t.tProjType;
				for (t.tP = 1; t.tP <= t.WeaponProjectileBase[t.tNewProjBase].cacheProjectile-1; t.tP++) //-1 already added one.
				{
					weapon_projectile_setup(&iSndForBaseSound, &iSndForBaseDestroy);
				}
				t.WeaponProjectileBase[t.tProjType].cacheLoaded = true;
			}

			t.tProjectileType=t.tProjType;
		}
	}
}

void weapon_projectile_free ( void )
{
	void DeleteEmitterEffects(uint32_t root);
	//  delete all projectiles (hide and reset them)
	for ( t.tProj = 1 ; t.tProj <= g.weaponSystem.numProjectiles; t.tProj++ )
	{
		if (  t.WeaponProjectile[t.tProj].activeFlag  ==  1 ) 
		{
			weapon_projectile_destroy ( );
		}
		t.WeaponProjectile[t.tProj].sourceEntity=0;
	}
}

// GG: whether an entity's file is in a folder of foliage (Foliage, Vegetation, Trees, Bush, Bushes, Plants, Shrubs, Grass),
// whose leaves would stop a projectile in mid air; the box test lets them through
static bool weapon_projectile_isfoliage ( int iEntID )
{
	char pPath[MAX_PATH];
	pPath[0] = '\\';
	strncpy ( pPath + 1, t.entitybank_s[iEntID].Get(), MAX_PATH - 2 );
	pPath[MAX_PATH - 1] = 0;
	for ( char* pChar = pPath; *pChar; pChar++ )
	{
		if ( *pChar == '/' ) *pChar = '\\';
		*pChar = (char)tolower ( (unsigned char)*pChar );
	}
	const char* pFolders[] = { "\\foliage\\", "\\vegetation\\", "\\trees\\", "\\bush\\", "\\bushes\\", "\\plants\\", "\\shrubs\\", "\\grass\\" };
	for ( int i = 0; i < sizeof(pFolders) / sizeof(pFolders[0]); i++ )
		if ( strstr ( pPath, pFolders[i] ) ) return true;
	return false;
}

// GG: a contact projectile's path this frame against the collision boxes of entities with no physics body, where they
// stand now: CollisionOff removes a moving vehicle's body, so the projectile's own body can't touch it. Each box is grown
// by the projectile's half size, and one the path starts inside is passed (the thrower's own vehicle), as is foliage.
// Returns the object of the first box entered (0 none), the entry point, how far along the path it is (0 to 1), and the
// normal of the face entered
static int weapon_projectile_boxhit ( GGVECTOR3 vecFrom, GGVECTOR3 vecTo, float fRadius, int iIgnoreObj, GGVECTOR3* pHit, float* pAlong, GGVECTOR3* pNormal )
{
	bool CalcObjectWorld ( sObject* pObject );
	GGVECTOR3 vecPath = vecTo - vecFrom;
	float fPathSquared = GGVec3Dot ( &vecPath, &vecPath );
	if ( fPathSquared < 0.000001f ) return 0;
	int iHitObj = 0;
	float fBest = 2.0f;
	for ( int e = 1; e <= g.entityelementlist; e++ )
	{
		int iEntID = t.entityelement[e].bankindex;
		int iObj = t.entityelement[e].obj;
		if ( iEntID <= 0 || iObj <= 0 || iObj == iIgnoreObj ) continue;
		if ( t.entityelement[e].active == 0 || t.entityelement[e].usingphysicsnow != 0 ) continue;
		if ( t.entityprofile[iEntID].ismarker != 0 || t.entityprofile[iEntID].ischaracter != 0 ) continue;
		if ( ObjectExist ( iObj ) == 0 || GetVisible ( iObj ) == 0 ) continue;
		sObject* pObject = GetObjectData ( iObj );
		if ( !pObject ) continue;

		// a sphere round the object's origin that holds its box, against the path
		const GGVECTOR3& vecMin = pObject->collision.vecMin;
		const GGVECTOR3& vecMax = pObject->collision.vecMax;
		GGVECTOR3 vecFar ( fabs(vecMin.x) > fabs(vecMax.x) ? fabs(vecMin.x) : fabs(vecMax.x),
			fabs(vecMin.y) > fabs(vecMax.y) ? fabs(vecMin.y) : fabs(vecMax.y),
			fabs(vecMin.z) > fabs(vecMax.z) ? fabs(vecMin.z) : fabs(vecMax.z) );
		const GGVECTOR3& vecScale = pObject->position.vecScale;
		float fScale = fabs(vecScale.x);
		if ( fabs(vecScale.y) > fScale ) fScale = fabs(vecScale.y);
		if ( fabs(vecScale.z) > fScale ) fScale = fabs(vecScale.z);
		float fReach = GGVec3Length ( &vecFar ) * fScale * 1.5f + fRadius;
		GGVECTOR3 vecOrigin ( ObjectPositionX(iObj), ObjectPositionY(iObj), ObjectPositionZ(iObj) );
		GGVECTOR3 vecToOrigin = vecOrigin - vecFrom;
		float fNearest = GGVec3Dot ( &vecToOrigin, &vecPath ) / fPathSquared;
		if ( fNearest < 0 ) fNearest = 0;
		if ( fNearest > 1 ) fNearest = 1;
		GGVECTOR3 vecGap = vecOrigin - ( vecFrom + vecPath * fNearest );
		if ( GGVec3Length ( &vecGap ) > fReach ) continue;
		if ( weapon_projectile_isfoliage ( iEntID ) ) continue;

		// the path in the object's own space against its box, by slabs
		CalcObjectWorld ( pObject );
		GGMATRIX matInverse;
		float fDeterminant = 0.0f;
		GGMatrixInverse ( &matInverse, &fDeterminant, &pObject->position.matWorld );
		if ( fabs(fDeterminant) < 0.000001f || fScale < 0.0001f ) continue;
		GGVECTOR3 vecLocalFrom, vecLocalTo;
		GGVec3TransformCoord ( &vecLocalFrom, &vecFrom, &matInverse );
		GGVec3TransformCoord ( &vecLocalTo, &vecTo, &matInverse );
		const float fLocalRadius = fRadius / fScale;
		const float fStart[3] = { vecLocalFrom.x, vecLocalFrom.y, vecLocalFrom.z };
		const float fDelta[3] = { vecLocalTo.x - vecLocalFrom.x, vecLocalTo.y - vecLocalFrom.y, vecLocalTo.z - vecLocalFrom.z };
		const float fLow[3] = { vecMin.x - fLocalRadius, vecMin.y - fLocalRadius, vecMin.z - fLocalRadius };
		const float fHigh[3] = { vecMax.x + fLocalRadius, vecMax.y + fLocalRadius, vecMax.z + fLocalRadius };
		float fEnter = -1.0e30f, fExit = 1.0e30f;
		int iEnterAxis = 1;
		float fEnterSide = 1.0f;
		bool bMissed = false;
		for ( int a = 0; a < 3 && !bMissed; a++ )
		{
			if ( fabs(fDelta[a]) < 0.000001f )
			{
				if ( fStart[a] < fLow[a] || fStart[a] > fHigh[a] ) bMissed = true;
				continue;
			}
			float fT1 = ( fLow[a] - fStart[a] ) / fDelta[a];
			float fT2 = ( fHigh[a] - fStart[a] ) / fDelta[a];
			if ( fT1 > fT2 ) { float fSwap = fT1; fT1 = fT2; fT2 = fSwap; }
			// moving up an axis it comes in through the low face, whose normal points down that axis
			if ( fT1 > fEnter ) { fEnter = fT1; iEnterAxis = a; fEnterSide = fDelta[a] > 0 ? -1.0f : 1.0f; }
			if ( fT2 < fExit ) fExit = fT2;
			if ( fEnter > fExit ) bMissed = true;
		}
		if ( bMissed || fEnter < 0 || fEnter > 1 || fEnter >= fBest ) continue;
		fBest = fEnter;
		iHitObj = iObj;
		*pHit = vecFrom + vecPath * fEnter;

		// the face's normal in the world, by the inverse's transpose so a scaled box keeps its faces' normals
		GGVECTOR3 vecLocalNormal ( iEnterAxis == 0 ? fEnterSide : 0.0f, iEnterAxis == 1 ? fEnterSide : 0.0f, iEnterAxis == 2 ? fEnterSide : 0.0f );
		GGMATRIX matInverseTranspose;
		GGMatrixTranspose ( &matInverseTranspose, &matInverse );
		GGVec3TransformNormal ( pNormal, &vecLocalNormal, &matInverseTranspose );
		GGVec3Normalize ( pNormal, pNormal );
	}
	*pAlong = fBest;
	return iHitObj;
}

// GG: the surface a projectile ended on, for weapon_projectileresult_make (which takes it): its normal facing back along
// the flight (0,0,0 when it ended on none), and whether the projectile's file stands its explosion effect on it
static GGVECTOR3 g_vecProjectileResultNormal ( 0, 0, 0 );
static bool g_bProjectileResultOnNormal = false;

// GG: the projectile event's surface normal, g_projectileevent_nx/ny/nz (M-LUA.cpp)
float g_fProjectileEventNormal[3] = { 0, 0, 0 };
static void weapon_projectile_setresultnormal ( GGVECTOR3 vecNormal, GGVECTOR3 vecTravel )
{
	g_vecProjectileResultNormal = GGVECTOR3 ( 0, 0, 0 );
	float fLength = GGVec3Length ( &vecNormal );
	if ( fLength > 0.001f )
	{
		vecNormal = vecNormal * ( 1.0f / fLength );
		if ( GGVec3Dot ( &vecNormal, &vecTravel ) > 0 ) vecNormal = vecNormal * -1.0f;
		g_vecProjectileResultNormal = vecNormal;
	}
	g_bProjectileResultOnNormal = t.WeaponProjectileBase[t.tProjType].wpeExplosionNormal != 0;
}

void weapon_projectile_loop ( void )
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	//PE: Support WPE particles here.
	
	//  processes all active projectiles
	t.tTimer = Timer();
	for ( t.tProj = 1 ; t.tProj <= g.weaponSystem.numProjectiles; t.tProj++ )
	{
		if (  t.WeaponProjectile[t.tProj].activeFlag  ==  1 ) 
		{
			t.tNewProj = t.tProj;

			//  get projectile base index
			t.tProjType = t.WeaponProjectile[t.tProj].baseType;
			t.tobj = t.WeaponProjectile[t.tProj].obj;

			//  save old position
			t.tXOldPos_f = t.WeaponProjectile[t.tProj].xPos_f;
			t.tYOldPos_f = t.WeaponProjectile[t.tProj].yPos_f;
			t.tZOldPos_f = t.WeaponProjectile[t.tProj].zPos_f;
			t.WeaponProjectile[t.tProj].xOldPos_f = t.tXOldPos_f;
			t.WeaponProjectile[t.tProj].yOldPos_f = t.tYOldPos_f;
			t.WeaponProjectile[t.tProj].zOldPos_f = t.tZOldPos_f;

			// projectile movement if not real physics
			if ( t.WeaponProjectileBase[t.tProjType].usingRealPhysics == 0 ) 
			{
				//  perform turning if projectile can turn (use tracer object for calculation simplicity)
				t.tXTurnSpeed_f = t.WeaponProjectile[t.tProj].xTurnSpeed_f;
				t.tYTurnSpeed_f = t.WeaponProjectile[t.tProj].yTurnSpeed_f;
				t.tZTurnSpeed_f = t.WeaponProjectile[t.tProj].zTurnSpeed_f;
				if (  t.tXTurnSpeed_f  !=  0 || t.tYTurnSpeed_f  !=  0 || t.tZTurnSpeed_f  !=  0 ) 
				{
					//  add to local rotations by speed
					t.WeaponProjectile[t.tProj].xTurn_f = t.WeaponProjectile[t.tProj].xTurn_f + t.tXTurnSpeed_f * t.ElapsedTime_f;
					t.WeaponProjectile[t.tProj].yTurn_f = t.WeaponProjectile[t.tProj].yTurn_f + t.tYTurnSpeed_f * t.ElapsedTime_f;
					t.WeaponProjectile[t.tProj].zTurn_f = t.WeaponProjectile[t.tProj].zTurn_f + t.tZTurnSpeed_f * t.ElapsedTime_f;
					//  rotate our tracer object
					t.tTracerObj = t.WeaponProjectile[t.tProj].tracerObj;
					PositionObject (  t.tTracerObj,t.tXOldPos_f,t.tYOldPos_f,t.tZOldPos_f );
					RotateObject (  t.tTracerObj,t.WeaponProjectile[t.tProj].xAng_f,t.WeaponProjectile[t.tProj].yAng_f,t.WeaponProjectile[t.tProj].zAng_f );
					RollObjectRight (  t.tTracerObj,t.WeaponProjectile[t.tProj].zTurn_f );
					TurnObjectRight (  t.tTracerObj,t.WeaponProjectile[t.tProj].yTurn_f );
					PitchObjectUp (  t.tTracerObj,t.WeaponProjectile[t.tProj].xTurn_f );
					//  move our tracer object forward so we can work out the new XYZ speed values
					t.tXSpd_f = t.WeaponProjectile[t.tProj].xSpeed_f;
					t.tYSpd_f = t.WeaponProjectile[t.tProj].ySpeed_f;
					t.tZSpd_f = t.WeaponProjectile[t.tProj].zSpeed_f;
					t.tTotalSpeed_f = Sqrt(t.tXSpd_f*t.tXSpd_f + t.tYSpd_f*t.tYSpd_f + t.tZSpd_f*t.tZSpd_f);
					MoveObject (  t.tTracerObj,t.tTotalSpeed_f );
					t.WeaponProjectile[t.tProj].xSpeed_f = ObjectPositionX(t.tTracerObj) - t.tXOldPos_f;
					t.WeaponProjectile[t.tProj].ySpeed_f = ObjectPositionY(t.tTracerObj) - t.tYOldPos_f;
					t.WeaponProjectile[t.tProj].zSpeed_f = ObjectPositionZ(t.tTracerObj) - t.tZOldPos_f;
					//  can scale projectile based on power of player
					if (  t.playercontrol.thirdperson.enabled == 1 ) 
					{
						t.tscaleme_f = (float)t.player[1].powers.level;
						ScaleObject (  t.tobj,t.tscaleme_f,t.tscaleme_f,t.tscaleme_f );
					}
				}

				if (t.WeaponProjectileBase[t.tProjType].iAtlasHeight > 0)
				{
					float fNextFrame = g.timeelapsed_f * t.WeaponProjectile[t.tProj].fDecalSpeed;
					t.WeaponProjectile[t.tProj].fDecalFrame = t.WeaponProjectile[t.tProj].fDecalFrame + fNextFrame;
					if (t.WeaponProjectile[t.tProj].fDecalFrame < 0) t.WeaponProjectile[t.tProj].fDecalFrame = 0;
					if (t.WeaponProjectile[t.tProj].fDecalFrame > (t.WeaponProjectileBase[t.tProjType].iAtlasHeight * t.WeaponProjectileBase[t.tProjType].iAtlasWidth))
						t.WeaponProjectile[t.tProj].fDecalFrame = 0;

					if (ObjectExist(t.tobj) == 1)
					{
						LockVertexDataForLimbCore(t.tobj, 0, 1);
						SetVertexDataNormals(0, 0, 1, 0);
						SetVertexDataNormals(1, 0, 1, 0);
						SetVertexDataNormals(2, 0, 1, 0);
						SetVertexDataNormals(3, 0, 1, 0);
						SetVertexDataNormals(4, 0, 1, 0);
						SetVertexDataNormals(5, 0, 1, 0);
						UnlockVertexData();
						SetObjectUVManually(t.tobj, t.WeaponProjectile[t.tProj].fDecalFrame, t.WeaponProjectileBase[t.tProjType].iAtlasWidth, t.WeaponProjectileBase[t.tProjType].iAtlasHeight);
					}
				}

				//  work out the new position using our speed and elapsed time
				t.tXNewPos_f = t.tXOldPos_f + t.WeaponProjectile[t.tProj].xSpeed_f * t.ElapsedTime_f;
				t.tYNewPos_f = t.tYOldPos_f + t.WeaponProjectile[t.tProj].ySpeed_f * t.ElapsedTime_f;
				t.tZNewPos_f = t.tZOldPos_f + t.WeaponProjectile[t.tProj].zSpeed_f * t.ElapsedTime_f;
				t.WeaponProjectile[t.tProj].xPos_f = t.tXNewPos_f;
				t.WeaponProjectile[t.tProj].yPos_f = t.tYNewPos_f;
				t.WeaponProjectile[t.tProj].zPos_f = t.tZNewPos_f;

				if (t.WeaponProjectile[t.tProj].WPE_Root > 0)
				{
					float addy = 2; //PE: To fit projectile.
					if (t.WeaponProjectileBase[t.tProjType].iAtlasHeight > 0)
						addy = 5;
					bool WickedCall_ParticleEffectPosition(uint32_t root, float fX, float fY, float fZ);
					uint32_t root = t.WeaponProjectile[t.tProj].WPE_Root;
					WickedCall_ParticleEffectPosition(root, t.tXNewPos_f, t.tYNewPos_f + addy, t.tZNewPos_f);
				}

			}
			else
			{
				//  get latest position from actual physics object
				t.tXNewPos_f = ObjectPositionX(t.tobj);
				t.tYNewPos_f = ObjectPositionY(t.tobj);
				t.tZNewPos_f = ObjectPositionZ(t.tobj);
				t.WeaponProjectile[t.tProj].xPos_f = t.tXNewPos_f;
				t.WeaponProjectile[t.tProj].yPos_f = t.tYNewPos_f;
				t.WeaponProjectile[t.tProj].zPos_f = t.tZNewPos_f;
				if (t.WeaponProjectile[t.tProj].WPE_Root > 0)
				{
					float addy = 2; //PE: To fit projectile.
					if (t.WeaponProjectileBase[t.tProjType].iAtlasHeight > 0)
						addy = 5;
					bool WickedCall_ParticleEffectPosition(uint32_t root, float fX, float fY, float fZ);
					uint32_t root = t.WeaponProjectile[t.tProj].WPE_Root;
					WickedCall_ParticleEffectPosition(root, t.tXNewPos_f, t.tYNewPos_f + addy, t.tZNewPos_f);
				}
			}

			//  create dynamic light effect from all projectile (uses last one)
			if ( t.WeaponProjectile[t.tProj].sourceEntity == 0 ) 
			{
				if ( t.tTimer > t.WeaponProjectile[t.tProj].createStamp + 100 ) 
				{
					if ( t.WeaponProjectile[t.tProj].usespotlighting != 0 ) 
					{
						//  only start dynamic light a split second after launch (so protagonist rear does not light up)
						t.tx_f=t.tXNewPos_f ; t.ty_f=t.tYNewPos_f ; t.tz_f=t.tZNewPos_f ; t.tmodeindex=t.tProj;
						lighting_spotflashtracking ( );
					}
				}
			}

			int custom_decal_explosion = 0;
			if (t.WeaponProjectileBase[t.tProjType].explosion_decalid > 0)
				custom_decal_explosion = t.WeaponProjectileBase[t.tProjType].explosion_decalid;

			//  has this projectile exceeded its life?
			t.tthisprojectileexploded=0;
			if (  t.tTimer > t.WeaponProjectile[t.tProj].createStamp + t.WeaponProjectileBase[t.tProjType].life ) 
			{
				//  run out of fuel
				if ( t.WeaponProjectileBase[t.tProjType].projectileEventType > 0 ) 
					t.tProjectileName_s = t.WeaponProjectileBase[t.tProjType].name_s;
				else
					t.tProjectileName_s = "";
				t.tProjectileResult = WEAPON_PROJECTILERESULT_EXPLODE;
				if ( t.tProjectileResult == 2 && t.WeaponProjectileBase[t.tProjType].explosionType == 99 ) 
				{
					t.tProjectileResult = WEAPON_PROJECTILERESULT_CUSTOM;
					t.tProjectileResultExplosionImageID = t.WeaponProjectileBase[t.tProjType].explosionImageID;
					t.tProjectileResultLightFlag = t.WeaponProjectileBase[t.tProjType].explosionLightFlag;
					t.tProjectileResultSmokeImageID = t.WeaponProjectileBase[t.tProjType].explosionSmokeImageID;
					t.tProjectileResultSparksCount = t.WeaponProjectileBase[t.tProjType].explosionSparksCount;
					t.tProjectileResultSize = t.WeaponProjectileBase[t.tProjType].explosionSize;
					t.tProjectileResultSmokeSize = t.WeaponProjectileBase[t.tProjType].explosionSmokeSize;
					t.tProjectileResultSparksSize = t.WeaponProjectileBase[t.tProjType].explosionSparksSize;
				}
				t.tx_f = t.tXNewPos_f; t.ty_f = t.tYNewPos_f; t.tz_f = t.tZNewPos_f;
				t.tDamage_f = t.WeaponProjectileBase[t.tProjType].damage_f;
				t.tradius_f = t.WeaponProjectileBase[t.tProjType].damageRadius_f;
				t.tSoundID = t.WeaponProjectile[t.tProj].soundDeath;
				t.tSourceEntity = t.WeaponProjectile[t.tProj].sourceEntity;
				t.tHitObj = 0;
				weapon_projectile_setresultnormal ( GGVECTOR3 ( 0, 0, 0 ), GGVECTOR3 ( 0, 0, 0 ) );
				weapon_projectileresult_make (custom_decal_explosion);
				weapon_projectile_destroy ( );
				t.tthisprojectileexploded=1;

				if (  t.game.runasmultiplayer == 1 ) 
				{
					t.tSteamBulletOn = 0;
					if (  t.WeaponProjectile[t.tNewProj].sourceEntity  ==  0 ) 
					{
						mp_update_projectile ( );
					}
				}
			}
			else
			{
				//  detect if projectile hits the player camera
				t.tdx_f=CameraPositionX(0)-t.tXNewPos_f;
				t.tdy_f=CameraPositionY(0)-t.tYNewPos_f;
				t.tdz_f=CameraPositionZ(0)-t.tZNewPos_f;
				t.tdd_f=t.tdx_f*t.tdx_f + t.tdy_f*t.tdy_f + t.tdz_f*t.tdz_f;
				if (  t.tdd_f<50.0*50.0 && t.playercontrol.thirdperson.enabled == 0 ) 
				{
					//  if close enough to player, and not fired by player, explode!
					if (  t.WeaponProjectile[t.tNewProj].sourceEntity>0 ) 
					{
						t.tHitX_f = t.tXOldPos_f;
						t.tHitY_f = t.tYOldPos_f;
						t.tHitZ_f = t.tZOldPos_f;
						if ( t.WeaponProjectileBase[t.tProjType].projectileEventType > 0 ) 
							t.tProjectileName_s = t.WeaponProjectileBase[t.tProjType].name_s;
						else
							t.tProjectileName_s = "";
						t.tProjectileResult = WEAPON_PROJECTILERESULT_EXPLODE;
						if ( t.tProjectileResult == 2 && t.WeaponProjectileBase[t.tProjType].explosionType == 99 ) 
						{
							t.tProjectileResult = WEAPON_PROJECTILERESULT_CUSTOM;
							t.tProjectileResultExplosionImageID = t.WeaponProjectileBase[t.tProjType].explosionImageID;
							t.tProjectileResultLightFlag = t.WeaponProjectileBase[t.tProjType].explosionLightFlag;
							t.tProjectileResultSmokeImageID = t.WeaponProjectileBase[t.tProjType].explosionSmokeImageID;
							t.tProjectileResultSparksCount = t.WeaponProjectileBase[t.tProjType].explosionSparksCount;
							t.tProjectileResultSize = t.WeaponProjectileBase[t.tProjType].explosionSize;
							t.tProjectileResultSmokeSize = t.WeaponProjectileBase[t.tProjType].explosionSmokeSize;
							t.tProjectileResultSparksSize = t.WeaponProjectileBase[t.tProjType].explosionSparksSize;
						}
						t.tx_f = t.tHitX_f; t.ty_f = t.tHitY_f; t.tz_f = t.tHitZ_f;
						t.tDamage_f = t.WeaponProjectileBase[t.tProjType].damage_f;
						t.tradius_f = t.WeaponProjectileBase[t.tProjType].damageRadius_f;
						t.tSoundID = t.WeaponProjectile[t.tProj].soundDeath;
						t.tSourceEntity = t.WeaponProjectile[t.tProj].sourceEntity;
						t.tHitObj = 0;
						weapon_projectile_setresultnormal ( GGVECTOR3 ( 0, 0, 0 ), GGVECTOR3 ( 0, 0, 0 ) );
						weapon_projectileresult_make (custom_decal_explosion);
						weapon_projectile_destroy ( );
						t.tthisprojectileexploded=1;

						if (  t.game.runasmultiplayer == 1 ) 
						{
							t.tSteamBulletOn = 0;
							if (  t.WeaponProjectile[t.tNewProj].sourceEntity  ==  0 ) 
							{
								mp_update_projectile ( );
							}
						}
					}
				}
				else
				{
					//  has this projectile hit the ground?
					if (  ODERayTerrain(t.tXOldPos_f,t.tYOldPos_f,t.tZOldPos_f,t.tXNewPos_f,t.tYNewPos_f,t.tZNewPos_f, false) == 1 ) 
					{
						//  hit the Floor (  )
						if (  t.WeaponProjectileBase[t.tProjType].resultBounce>0 ) 
						{
							t.tHitX_f=ODEGetRayCollisionX();
							t.tHitY_f=ODEGetRayCollisionY();
							t.tHitZ_f=ODEGetRayCollisionZ();
							if ( t.WeaponProjectileBase[t.tProjType].projectileEventType > 0 ) 
								t.tProjectileName_s = t.WeaponProjectileBase[t.tProjType].name_s;
							else
								t.tProjectileName_s = "";
							t.tProjectileResult = t.WeaponProjectileBase[t.tProjType].resultBounce;
							if ( t.tProjectileResult == 2 && t.WeaponProjectileBase[t.tProjType].explosionType == 99 ) 
							{
								t.tProjectileResult = WEAPON_PROJECTILERESULT_CUSTOM;
								t.tProjectileResultExplosionImageID = t.WeaponProjectileBase[t.tProjType].explosionImageID;
								t.tProjectileResultLightFlag = t.WeaponProjectileBase[t.tProjType].explosionLightFlag;
								t.tProjectileResultSmokeImageID = t.WeaponProjectileBase[t.tProjType].explosionSmokeImageID;
								t.tProjectileResultSparksCount = t.WeaponProjectileBase[t.tProjType].explosionSparksCount;
								t.tProjectileResultSize = t.WeaponProjectileBase[t.tProjType].explosionSize;
								t.tProjectileResultSmokeSize = t.WeaponProjectileBase[t.tProjType].explosionSmokeSize;
								t.tProjectileResultSparksSize = t.WeaponProjectileBase[t.tProjType].explosionSparksSize;
							}
							t.tx_f = t.tHitX_f; t.ty_f = t.tHitY_f; t.tz_f = t.tHitZ_f;
							t.tDamage_f = t.WeaponProjectileBase[t.tProjType].damage_f;
							t.tradius_f = t.WeaponProjectileBase[t.tProjType].damageRadius_f;
							t.tSoundID = t.WeaponProjectile[t.tProj].soundDeath;
							t.tSourceEntity = t.WeaponProjectile[t.tProj].sourceEntity;
							t.tHitObj = 0;
							// the ground's normal from the ray that met it
							GGVECTOR3 vecTravel ( t.tXNewPos_f - t.tXOldPos_f, t.tYNewPos_f - t.tYOldPos_f, t.tZNewPos_f - t.tZOldPos_f );
							weapon_projectile_setresultnormal ( GGVECTOR3 ( ODEGetRayNormalX(), ODEGetRayNormalY(), ODEGetRayNormalZ() ), vecTravel );
							weapon_projectileresult_make (custom_decal_explosion);
							weapon_projectile_destroy ( );
							t.tthisprojectileexploded=1;

							if (  t.game.runasmultiplayer == 1 ) 
							{
								t.tSteamBulletOn = 0;
								if (  t.WeaponProjectile[t.tNewProj].sourceEntity  ==  0 ) 
								{
									mp_update_projectile ( );
								}
							}
						}
					}
					else
					{
						//  has this projectile gone underwater?
						if (  t.tYNewPos_f < t.terrain.waterliney_f ) 
						{
							//  yes it has
							t.tXPos_f = t.tXNewPos_f; t.tZPos_f = t.tZNewPos_f ; t.tsize_f = 1.5;
							weapon_projectile_destroy ( );
							//  trigger splash
							g.decalx = t.tXNewPos_f ; g.decaly = t.terrain.waterliney_f + 1; g.decalz = t.tZNewPos_f; t.tInScale_f = 3;
							decal_triggerwatersplash ( );
							//  trigger sound
							t.tmatindex=17; 
							t.tsoundtrigger=t.material[t.tmatindex].matsound_id[matSound_LandHard][0];
							t.tspd_f=(t.material[t.tmatindex].freq*1.0f)+Rnd(t.material[t.tmatindex].freq)*0.5f;
							t.tsx_f=t.tXNewPos_f ; t.tsy_f=t.terrain.waterliney_f ; t.tsz_f=t.tZNewPos_f;
							t.tvol_f = 100.0f ; material_triggersound ( 0 );
							t.tthisprojectileexploded=1;

							if (  t.game.runasmultiplayer == 1 ) 
							{
								t.tSteamBulletOn = 0;
								if (  t.WeaponProjectile[t.tNewProj].sourceEntity  ==  0 ) 
								{
									mp_update_projectile ( );
								}
							}
						}
						else
						{
							//  check for hit with entities
							t.ttdx_f=t.tXNewPos_f-t.tXOldPos_f;
							t.ttdy_f=t.tYNewPos_f-t.tYOldPos_f;
							t.ttdz_f=t.tZNewPos_f-t.tZOldPos_f;
							t.ttdd_f=Sqrt(abs(t.ttdx_f*t.ttdx_f)+abs(t.ttdy_f*t.ttdy_f)+abs(t.ttdz_f*t.ttdz_f));
							t.ttdx_f=t.ttdx_f/t.ttdd_f;
							t.ttdy_f=t.ttdy_f/t.ttdd_f;
							t.ttdz_f=t.ttdz_f/t.ttdd_f;
							t.tXOldPos_f=t.tXOldPos_f-(t.ttdx_f*60.0f);
							t.tYOldPos_f=t.tYOldPos_f-(t.ttdy_f*60.0f);
							t.tZOldPos_f=t.tZOldPos_f-(t.ttdz_f*60.0f);
							//  ignore the entity object which fires it
							t.tIgnoreObject = 0;
							if (  t.WeaponProjectile[t.tProj].sourceEntity>0  )  t.tIgnoreObject  =  t.entityelement[t.WeaponProjectile[t.tProj].sourceEntity].obj;
							t.tEndEntity = g.entityviewstartobj+g.entityelementlist;
							//Dave - don't need to do raycasts for the nade hitting anything as it is controlled by physics
							bool bContactHit = false;
							float fContactX = 0, fContactY = 0, fContactZ = 0;
							GGVECTOR3 vecContactNormal ( 0, 0, 0 );
							GGVECTOR3 vecRayNormal ( 0, 0, 0 );
							if ( t.WeaponProjectileBase[t.tProjType].resultBounce>0 && t.WeaponProjectileBase[t.tProjType].usingRealPhysics>0 )
							{
								// GG: a real physics one ends where its body first touched another (ODEWatchProjectileContact), the
								// object touched named if it is an entity's; the scene picks below missed what its body bounced off
								int iTouched = ODEGetProjectileContact ( t.tobj, &fContactX, &fContactY, &fContactZ, &vecContactNormal.x, &vecContactNormal.y, &vecContactNormal.z );
								bContactHit = iTouched >= 0;
								t.tHitObj = iTouched > 0 ? iTouched : 0;

								// and the boxes of entities with no body (a moving vehicle), if its path entered one first
								GGVECTOR3 vecFrom ( t.WeaponProjectile[t.tProj].xOldPos_f, t.WeaponProjectile[t.tProj].yOldPos_f, t.WeaponProjectile[t.tProj].zOldPos_f );
								GGVECTOR3 vecTo ( t.tXNewPos_f, t.tYNewPos_f, t.tZNewPos_f );
								float fRadius = ObjectSizeX ( t.tobj, 1 );
								if ( ObjectSizeY ( t.tobj, 1 ) < fRadius ) fRadius = ObjectSizeY ( t.tobj, 1 );
								if ( ObjectSizeZ ( t.tobj, 1 ) < fRadius ) fRadius = ObjectSizeZ ( t.tobj, 1 );
								fRadius *= 0.5f;
								GGVECTOR3 vecBoxHit ( 0, 0, 0 );
								GGVECTOR3 vecBoxNormal ( 0, 0, 0 );
								float fBoxAlong = 2.0f;
								int iBoxObj = weapon_projectile_boxhit ( vecFrom, vecTo, fRadius, t.tIgnoreObject, &vecBoxHit, &fBoxAlong, &vecBoxNormal );
								if ( iBoxObj > 0 )
								{
									float fContactAlong = 2.0f;
									if ( bContactHit )
									{
										GGVECTOR3 vecPath = vecTo - vecFrom;
										GGVECTOR3 vecToContact = GGVECTOR3 ( fContactX, fContactY, fContactZ ) - vecFrom;
										float fPathSquared = GGVec3Dot ( &vecPath, &vecPath );
										fContactAlong = fPathSquared > 0.000001f ? GGVec3Dot ( &vecToContact, &vecPath ) / fPathSquared : 0.0f;
									}
									if ( fBoxAlong <= fContactAlong )
									{
										bContactHit = true;
										t.tHitObj = iBoxObj;
										fContactX = vecBoxHit.x;
										fContactY = vecBoxHit.y;
										fContactZ = vecBoxHit.z;
										vecContactNormal = vecBoxNormal;
									}
								}
							}
							else if ( t.WeaponProjectileBase[t.tProjType].resultBounce>0 )
							{
								if (g.lightmappedobjectoffset >= g.lightmappedobjectoffsetfinish)
									t.ttt = IntersectAll(87000, 87000 + g.merged_new_objects - 1, 0, 0, 0, 0, 0, 0, -123);
								else
									t.ttt=IntersectAll(g.lightmappedobjectoffset,g.lightmappedobjectoffsetfinish,0,0,0,0,0,0,-123);
								t.tHitObj=IntersectAll(g.entityviewstartobj,g.entityviewendobj,t.tXOldPos_f,t.tYOldPos_f,t.tZOldPos_f,t.tXNewPos_f,t.tYNewPos_f,t.tZNewPos_f,t.tIgnoreObject);
								if ( t.tHitObj > 0 ) vecRayNormal = GGVECTOR3 ( ChecklistFValueA(7), ChecklistFValueB(7), ChecklistFValueC(7) );

								// GG: the ray meets only bodies, so an entity with none (CollisionOff, a vehicle; physics off, a prop)
								// let the projectile through: its true path this frame against their boxes too, the nearer of the
								// box entered and the ray's hit taken, a box ending it at the entry
								GGVECTOR3 vecFrom ( t.WeaponProjectile[t.tProj].xOldPos_f, t.WeaponProjectile[t.tProj].yOldPos_f, t.WeaponProjectile[t.tProj].zOldPos_f );
								GGVECTOR3 vecTo ( t.tXNewPos_f, t.tYNewPos_f, t.tZNewPos_f );
								float fRadius = 0.0f;
								if ( t.tobj > 0 && ObjectExist ( t.tobj ) == 1 )
								{
									fRadius = ObjectSizeX ( t.tobj, 1 );
									if ( ObjectSizeY ( t.tobj, 1 ) < fRadius ) fRadius = ObjectSizeY ( t.tobj, 1 );
									if ( ObjectSizeZ ( t.tobj, 1 ) < fRadius ) fRadius = ObjectSizeZ ( t.tobj, 1 );
									fRadius *= 0.5f;
								}
								GGVECTOR3 vecBoxHit ( 0, 0, 0 );
								GGVECTOR3 vecBoxNormal ( 0, 0, 0 );
								float fBoxAlong = 2.0f;
								int iBoxObj = weapon_projectile_boxhit ( vecFrom, vecTo, fRadius, t.tIgnoreObject, &vecBoxHit, &fBoxAlong, &vecBoxNormal );
								if ( iBoxObj > 0 )
								{
									float fRayAlong = 2.0f;
									if ( t.tHitObj > 0 )
									{
										// the ray starts 60 units back, so a hit before this frame's path counts as at its start
										GGVECTOR3 vecPath = vecTo - vecFrom;
										GGVECTOR3 vecToRayHit = GGVECTOR3 ( ChecklistFValueA(6), ChecklistFValueB(6), ChecklistFValueC(6) ) - vecFrom;
										float fPathSquared = GGVec3Dot ( &vecPath, &vecPath );
										fRayAlong = fPathSquared > 0.000001f ? GGVec3Dot ( &vecToRayHit, &vecPath ) / fPathSquared : 0.0f;
										if ( fRayAlong < 0.0f ) fRayAlong = 0.0f;
									}
									if ( fBoxAlong <= fRayAlong )
									{
										bContactHit = true;
										t.tHitObj = iBoxObj;
										fContactX = vecBoxHit.x;
										fContactY = vecBoxHit.y;
										fContactZ = vecBoxHit.z;
										vecContactNormal = vecBoxNormal;
									}
								}
							}
							else
								t.tHitObj = 0;

							// 291015 - in addition, use a distance check to explode projectiles when they are close to enemies
							int iIgnoreThirdPersonEntityCharacter = -1;
							if (  t.playercontrol.thirdperson.enabled == 1 ) 
								iIgnoreThirdPersonEntityCharacter = t.playercontrol.thirdperson.charactere;

							bool bDirectHitOnObjectItself = false;
							for ( t.ttte = 1; t.ttte <= g.entityelementlist; t.ttte++ )
							{
								t.tentid=t.entityelement[t.ttte].bankindex;
								if (  t.entityprofile[t.tentid].ischaracter == 1 ) 
								{
									if (  t.tentid > 0 && t.entityelement[t.ttte].health > 0 && t.ttte != iIgnoreThirdPersonEntityCharacter ) 
									{
										int tobj =  t.entityelement[t.ttte].obj;
										if ( tobj != t.tIgnoreObject )
										{
											float fCharacterMidrift = GetObjectCollisionCenterY(tobj);
											t.tdx_f = t.entityelement[t.ttte].x - t.tXNewPos_f;
											t.tdy_f = (t.entityelement[t.ttte].y + fCharacterMidrift) - t.tYNewPos_f;
											t.tdz_f = t.entityelement[t.ttte].z - t.tZNewPos_f;
											float fDist = sqrt( (t.tdx_f*t.tdx_f)+(t.tdy_f*t.tdy_f)+(t.tdz_f*t.tdz_f) );
											if ( fDist < 40.0f )
											{
												fDist = sqrt( (t.tdx_f*t.tdx_f)+(t.tdz_f*t.tdz_f) );
												if ( fDist < 30.0f )
												{
													// projectile close enough to enemy to cause a hit
													t.tHitObj = t.entityelement[t.ttte].obj;
													bDirectHitOnObjectItself = true;
												}
											}
										}
									}
								}
							}


							//  if third person, ignore the protagonist (but only if this was PLAYERS PROJECTILE)
							if (  t.playercontrol.thirdperson.enabled == 1 ) 
							{
								t.tpersone=t.playercontrol.thirdperson.charactere;
								if (  t.tpersone>0 ) 
								{
									t.tpersonobj=t.entityelement[t.tpersone].obj;
									if (  ObjectExist(t.tpersonobj) == 1 && t.tpersonobj == t.tHitObj ) 
									{
										if (  t.WeaponProjectile[t.tNewProj].sourceEntity == 0 ) 
										{
											t.tHitObj=0;
										}
									}
								}
							}

							//  if confirmed, bang!
							if (  t.tHitObj>0 || bContactHit )
							{

								//  only explode if flagged to do so on a hit
								t.tsteamLastHit = t.tHitObj;
								if (  t.WeaponProjectileBase[t.tProjType].resultBounce>0 ) 
								{
									// 091115 - direct it or an intersect coordinate
									if ( bDirectHitOnObjectItself==true )
									{
										// make hit coordinate just in front of enemy towards player camera
										t.tHitX_f = ObjectPositionX(t.tHitObj);
										t.tHitY_f = ObjectPositionY(t.tHitObj);
										t.tHitZ_f = ObjectPositionZ(t.tHitObj);
										float fDX = CameraPositionX() - t.tHitX_f;
										float fDZ = CameraPositionZ() - t.tHitZ_f;
										float fDist = sqrt( (fDX*fDX) + (fDZ*fDZ) );
										fDX /= fDist;
										fDZ /= fDist;
										t.tHitX_f += (fDX*10.0f);
										t.tHitZ_f += (fDZ*10.0f);
									}
									else if ( bContactHit == true )
									{
										t.tHitX_f = fContactX;
										t.tHitY_f = fContactY;
										t.tHitZ_f = fContactZ;
									}
									else
									{
										t.tHitX_f=ChecklistFValueA(6);
										t.tHitY_f=ChecklistFValueB(6);
										t.tHitZ_f=ChecklistFValueC(6);
									}
									if ( t.WeaponProjectileBase[t.tProjType].projectileEventType > 0 ) 
										t.tProjectileName_s = t.WeaponProjectileBase[t.tProjType].name_s;
									else
										t.tProjectileName_s = "";
									t.tProjectileResult = t.WeaponProjectileBase[t.tProjType].resultBounce;
									if ( t.tProjectileResult == 2 && t.WeaponProjectileBase[t.tProjType].explosionType == 99 ) 
									{
										t.tProjectileResult = WEAPON_PROJECTILERESULT_CUSTOM;
										t.tProjectileResultExplosionImageID = t.WeaponProjectileBase[t.tProjType].explosionImageID;
										t.tProjectileResultLightFlag = t.WeaponProjectileBase[t.tProjType].explosionLightFlag;
										t.tProjectileResultSmokeImageID = t.WeaponProjectileBase[t.tProjType].explosionSmokeImageID;
										t.tProjectileResultSparksCount = t.WeaponProjectileBase[t.tProjType].explosionSparksCount;
										t.tProjectileResultSize = t.WeaponProjectileBase[t.tProjType].explosionSize;
										t.tProjectileResultSmokeSize = t.WeaponProjectileBase[t.tProjType].explosionSmokeSize;
										t.tProjectileResultSparksSize = t.WeaponProjectileBase[t.tProjType].explosionSparksSize;
									}
									t.tx_f = t.tHitX_f; t.ty_f = t.tHitY_f; t.tz_f = t.tHitZ_f;
									t.tDamage_f = t.WeaponProjectileBase[t.tProjType].damage_f;
									t.tradius_f = t.WeaponProjectileBase[t.tProjType].damageRadius_f;
									t.tSoundID = t.WeaponProjectile[t.tProj].soundDeath;
									t.tSourceEntity = t.WeaponProjectile[t.tProj].sourceEntity;
									// the surface's normal: the contact's or box face's, else the ray's; none for a burst beside a character
									GGVECTOR3 vecTravel ( t.tXNewPos_f - t.tXOldPos_f, t.tYNewPos_f - t.tYOldPos_f, t.tZNewPos_f - t.tZOldPos_f );
									GGVECTOR3 vecSurfaceNormal = bDirectHitOnObjectItself ? GGVECTOR3 ( 0, 0, 0 ) : ( bContactHit ? vecContactNormal : vecRayNormal );
									weapon_projectile_setresultnormal ( vecSurfaceNormal, vecTravel );
									weapon_projectileresult_make (custom_decal_explosion);
									weapon_projectile_destroy ( );
									t.tthisprojectileexploded=1;
									if (  t.game.runasmultiplayer == 1 ) 
									{
										t.tSteamBulletOn = 0;
										if (  t.WeaponProjectile[t.tNewProj].sourceEntity  ==  0 ) 
										{
											mp_update_projectile ( );
										}
									}
								}
							}
						}
					}
				}
			}

			//  if projectile has not been exploded/removed
			if (  t.tthisprojectileexploded == 0 ) 
			{
				//  update physical object if one exists (and projectile movement if not real physics)
				if (  t.WeaponProjectileBase[t.tProjType].usingRealPhysics == 0 ) 
				{
					if (  t.tobj > 0 ) 
					{
						PositionObject (  t.tobj,t.WeaponProjectile[t.tProj].xPos_f,t.WeaponProjectile[t.tProj].yPos_f,t.WeaponProjectile[t.tProj].zPos_f );
						RotateObject (  t.tobj,t.WeaponProjectile[t.tProj].xAng_f,t.WeaponProjectile[t.tProj].yAng_f,t.WeaponProjectile[t.tProj].zAng_f );
					}
				}

				//  tracer logic
				if (  t.WeaponProjectile[t.tProj].tracerFlag  ==  1 ) 
				{
					ShowObject (  t.WeaponProjectile[t.tProj].tracerObj );
				}

				//  do we need to reposition the 3D sound?
				t.tSndID = t.WeaponProjectile[t.tProj].sound;
				if (  t.tSndID > 0 ) 
				{
					if (  t.WeaponProjectileBase[t.tProjType].soundLoopFlag  ==  1 ) 
					{
						//  position the sound
						PositionSound (  t.tSndID,t.tXNewPos_f,t.tYNewPos_f,t.tZNewPos_f );
						//  calculate and set doppler pitch
						if (  t.WeaponProjectileBase[t.tProjType].soundDopplerFlag  ==  1 ) 
						{
							t.tOldDist_f = t.WeaponProjectile[t.tProj].soundDistFromPlayer_f;
							weapon_projectile_setDistFromPlayer ( );
							t.tDistDiff_f = t.tOldDist_f - t.WeaponProjectile[t.tProj].soundDistFromPlayer_f;
							t.tSoundMultiplier_f = 1 + (t.tDistDiff_f/t.ElapsedTime_f)*0.00015f;
							if (  t.tSoundMultiplier_f < 0.5f  )  t.tSoundMultiplier_f  =  0.5f;
							if (  t.tSoundMultiplier_f > 2  )  t.tSoundMultiplier_f  =  2;
							t.tsoundfreq = (int)(t.WeaponProjectileBase[t.tProjType].soundDopplerBaseSpeed * t.tSoundMultiplier_f);
							if (  t.tsoundfreq<10000  )  t.tsoundfreq = 10000;
							if (  t.tsoundfreq>60000  )  t.tsoundfreq = 60000;
							SetSoundSpeed (  t.tSndID,t.tsoundfreq );
						}
					}
				}

				if (  t.game.runasmultiplayer == 1 ) 
				{
					if (  t.WeaponProjectile[t.tNewProj].sourceEntity  ==  0 ) 
					{
						t.WeaponProjectile[t.tProj].xAng_f = ObjectAngleX(t.tobj);
						t.WeaponProjectile[t.tProj].yAng_f = ObjectAngleY(t.tobj);
						t.WeaponProjectile[t.tProj].zAng_f = ObjectAngleZ(t.tobj);
						t.tSteamBulletOn = 1;
						mp_update_projectile ( );
					}
				}
			}
		}
	}
}

void weapon_projectile_activatedecals(void)
{
	for (t.tProj = 1; t.tProj <= g.weaponSystem.numProjectiles; t.tProj++)
	{
		if (t.WeaponProjectile[t.tProj].obj > 0)
		{
			t.tProjectileType = t.WeaponProjectile[t.tProj].baseType;

			if (t.WeaponProjectileBase[t.tProjectileType].WPE_Effect.Len() > 0)
			{
				t.decal_s = t.WeaponProjectileBase[t.tProjectileType].WPE_Effect;
				decal_find();
				if (t.decalid < 0)  t.decalid = 0;
				if (t.decalid > 0)
				{
					t.WeaponProjectileBase[t.tProjectileType].decalid = t.decalid;
					t.decal[t.decalid].active = 1;
				}
			}
			if (t.WeaponProjectileBase[t.tProjectileType].WPE_Explosion.Len() > 0)
			{
				t.decal_s = t.WeaponProjectileBase[t.tProjectileType].WPE_Explosion;
				decal_find();
				if (t.decalid < 0)  t.decalid = 0;
				if (t.decalid > 0)
				{
					t.WeaponProjectileBase[t.tProjectileType].explosion_decalid = t.decalid;
					t.decal[t.decalid].active = 1;
				}
			}
		}
	}
}

void weapon_projectile_load ( void )
{
	//  add a projectile type to the system. Takes; tProjectileName$
	//  returns tResult, which is 0 (failed) or the index in WeaponProjectileBase() array

	// three projectiles are considered defaults, so can pass in a short hand version 
	cstr tProjectileFolder_s = t.tProjectileName_s + "\\";
	if ( strcmp ( Lower(t.tProjectileName_s.Get()), "modern\\handgrenade" ) == NULL ) t.tProjectileName_s = "handgrenade";
	if ( strcmp ( Lower(t.tProjectileName_s.Get()), "modern\\rpggrenade" ) == NULL ) t.tProjectileName_s = "rpggrenade";
	if ( strcmp ( Lower(t.tProjectileName_s.Get()), "fantasy\\fireball" ) == NULL ) t.tProjectileName_s = "fireball";

	// get name only
	cstr tProjectileNameOnly_s = t.tProjectileName_s;
	LPSTR pProjectileFolderName = t.tProjectileName_s.Get();
	for ( int n = strlen(pProjectileFolderName)-1; n > 0; n-- )
	{
		if ( pProjectileFolderName[n] == '\\' )
		{
			tProjectileNameOnly_s = cstr(pProjectileFolderName+n+1);
			break;
		}
	}

	//  find a free slot to load this definition in, or quit if it already exists and return the ID
	t.tNewProjBase = 0;
	for ( t.tP = 1 ; t.tP <= g.weaponSystem.numProjectileBases; t.tP++ )
	{
		if (  t.WeaponProjectileBase[t.tP].name_s == t.tProjectileName_s ) 
		{
			t.tResult = t.tP ; return;
		}
		else
		{
			if (  t.WeaponProjectileBase[t.tP].activeFlag  ==  0 ) 
			{
				t.tNewProjBase = t.tP;
				break;
			}
		}
	}

	//  if tNewProjBase is still zero, we didn't have enough slots
	if ( t.tNewProjBase == 0 ) 
	{
		t.tResult = 0; 
		return;
	}

	//  load in our Text (  file )
	t.tTextFile_s=cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + tProjectileNameOnly_s + ".txt";
	weapon_loadtextfile ( );
	if (  t.tResult  ==  0 ) 
	{
		t.tResult = 0; return;
	}

	//  read Text (  file and setup our projectile values
	t.WeaponProjectileBase[t.tNewProjBase].name_s = t.tProjectileName_s;
	t.tInField_s = "mode" ; weapon_readfield() ; t.WeaponProjectileBase[t.tNewProjBase].mode=(int)t.value1_f;
	t.tInField_s = "scaleMinX" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].baseObjScaleMinX_f = t.value1_f;
	t.tInField_s = "scaleMaxX" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].baseObjScaleMaxX_f = t.value1_f ;
	t.tInField_s = "scaleMinY" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].baseObjScaleMinY_f = t.value1_f ;
	t.tInField_s = "scaleMaxY" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].baseObjScaleMaxY_f = t.value1_f ;
	t.tInField_s = "scaleMinZ" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].baseObjScaleMinZ_f = t.value1_f ;
	t.tInField_s = "scaleMaxZ" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].baseObjScaleMaxZ_f = t.value1_f ;
	t.tInField_s = "avoidPlayerPenetration" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].avoidPlayerPenetration_f = t.value1_f ;
	t.tInField_s = "attachToWeaponLimb"; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].attachToWeaponLimb = (int)t.value1_f;
	t.tInField_s = "vert"; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].vert_f = t.value1_f;	
	t.tInField_s = "accuracyX" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].xAccuracy_f = t.value1_f ;
	t.tInField_s = "accuracyY" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].yAccuracy_f = t.value1_f ;
	t.tInField_s = "life" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].life = (int)t.value1_f ;
	t.tInField_s = "resultEndOfLife" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].resultEndOfLife = (int)t.value1_f ;
	t.tInField_s = "damage" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].damage_f = t.value1_f ;
	t.tInField_s = "damageRandom" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].damageRandom_f = t.value1_f ;
	t.tInField_s = "range" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].range_f = t.value1_f ;
	t.tInField_s = "fullDamageRange" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].fullDamageRange_f = t.value1_f ;
	t.tInField_s = "damageRadius" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].damageRadius_f = t.value1_f ;
	t.tInField_s = "resultIfDamaged" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].resultIfDamaged = (int)t.value1_f ;
	t.tInField_s = "resultIfDamagedDelay" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].resultIfDamagedDelay = (int)t.value1_f ;
	t.tInField_s = "resultOnInterval" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].resultOnInterval = (int)t.value1_f ;
	t.tInField_s = "resultOnIntervalTime" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].resultOnIntervalTime = (int)t.value1_f ;
	t.tInField_s = "resultBounce" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].resultBounce = (int)t.value1_f ;
	t.tInField_s = "thrustTime" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].thrustTime = (int)t.value1_f ;
	t.tInField_s = "thrustDelay" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].thrustDelay = (int)t.value1_f ;
	t.tInField_s = "speedMin" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedMin_f = t.value1_f ;
	t.tInField_s = "speedMax" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedMax_f = t.value1_f ;
	t.tInField_s = "speedAngMinX" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedAngMinX_f = t.value1_f ;
	t.tInField_s = "speedAngMaxX" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedAngMaxX_f = t.value1_f ;
	t.tInField_s = "speedAngMinY" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedAngMinY_f = t.value1_f ;
	t.tInField_s = "speedAngMaxY" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedAngMaxY_f = t.value1_f ;
	t.tInField_s = "speedAngMinZ" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedAngMinZ_f = t.value1_f ;
	t.tInField_s = "speedAngMaxZ" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedAngMaxZ_f = t.value1_f ;
	t.tInField_s = "speedTurnMinX" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedTurnMinX_f = t.value1_f ;
	t.tInField_s = "speedTurnMaxX" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedTurnMaxX_f = t.value1_f ;
	t.tInField_s = "speedTurnMinY" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedTurnMinY_f = t.value1_f ;
	t.tInField_s = "speedTurnMaxY" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedTurnMaxY_f = t.value1_f ;
	t.tInField_s = "speedTurnMinZ" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedTurnMinZ_f = t.value1_f ;
	t.tInField_s = "speedTurnMaxZ" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].speedTurnMaxZ_f = t.value1_f ;
	t.tInField_s = "usingRealPhysics" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].usingRealPhysics = t.value1_f ;
	t.tInField_s = "usingRealPhysicsAdjustYAngle" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].usingRealPhysicsAdjustYAngle = t.value1_f ;
	t.tInField_s = "usingRealPhysicsAdjustXAngle" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].usingRealPhysicsAdjustXAngle = t.value1_f ;
	t.tInField_s = "gravityModifier" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].gravityModifier_f = t.value1_f ;
	t.tInField_s = "airFrictionModifier" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].airFrictionModifier_f = t.value1_f ;
	t.tInField_s = "groundFrictionModifier" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].groundFrictionModifier_f = t.value1_f ;
	t.tInField_s = "thrustModifier" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].thrustModifier_f = t.value1_f ;
	t.tInField_s = "bounceFlag" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].bounceFlag = (int)t.value1_f ;
	t.tInField_s = "bounceModifier" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].bounceModifier_f = t.value1_f ;
	t.tInField_s = "bounceResult" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].bounceResult = (int)t.value1_f ;
	t.tInField_s = "soundLoopFlag" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].soundLoopFlag = (int)t.value1_f ;
	t.tInField_s = "soundDopplerFlag" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].soundDopplerFlag = (int)t.value1_f ;
	t.tInField_s = "soundDopplerBaseSpeed" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].soundDopplerBaseSpeed = (int)t.value1_f ;
	t.tInField_s = "soundInterval" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].soundInterval = (int)t.value1_f ;
	t.tInField_s = "overridespotlighting" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].overridespotlighting = (int)t.value1_f ;
	t.tInField_s = "noZWrite" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].noZWrite = (int)t.value1_f ;
	
	t.tInField_s = "particleType" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].particleType = (int)t.value1_f;
	t.tInField_s = "particleName" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].particleName = t.value_s;
	t.tInField_s = "explosionType" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].explosionType = (int)t.value1_f;
	t.tInField_s = "explosionName" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].explosionName = t.value_s;
	t.tInField_s = "explosionLightFlag" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].explosionLightFlag = (int)t.value1_f;
	t.tInField_s = "explosionSmokeName" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].explosionSmokeName = t.value_s;
	t.tInField_s = "explosionSparksCount" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].explosionSparksCount = (int)t.value1_f;
	t.tInField_s = "explosionSize" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].explosionSize = (int)t.value1_f;
	t.tInField_s = "explosionSmokeSize" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].explosionSmokeSize = (int)t.value1_f;
	t.tInField_s = "explosionSparksSize" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].explosionSparksSize = (int)t.value1_f;
	t.tInField_s = "projectileEventType" ; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].projectileEventType = (int)t.value1_f;

	t.tInField_s = "wpeeffect"; weapon_readfield();
	if (strlen(t.value_s.Get()) == 0)
	{
		t.WeaponProjectileBase[t.tNewProjBase].WPE_Effect = "";
		t.WeaponProjectileBase[t.tNewProjBase].WPE_MeshID = -1;
	}
	else
	{
		t.WeaponProjectileBase[t.tNewProjBase].WPE_Effect = t.value_s;
	}
	t.tInField_s = "wpeexplosion"; weapon_readfield();
	if (strlen(t.value_s.Get()) == 0)
	{
		t.WeaponProjectileBase[t.tNewProjBase].WPE_Explosion = "";
	}
	else
	{
		t.WeaponProjectileBase[t.tNewProjBase].WPE_Explosion = t.value_s;
	}
	// GG: 1 stands that effect on the surface the projectile hit (its local Y along the surface's normal), not upright
	t.tInField_s = "wpeexplosionnormal"; weapon_readfield(); t.WeaponProjectileBase[t.tNewProjBase].wpeExplosionNormal = (int)t.value1_f;

	t.tInField_s = "effect"; weapon_readfield();
	if (t.value_s == "decal_animate8.fx")
	{
		//PE: Support classic atlas effects.
		t.WeaponProjectileBase[t.tNewProjBase].iAtlasHeight = 8;
		t.WeaponProjectileBase[t.tNewProjBase].iAtlasWidth = 8;
	}


	t.tInField_s = "object" ; weapon_readfield ( );
	if (strlen(t.value_s.Get()) == 0)
	{
		// no object associated, use blank object so can continue
		weapon_getfreeobject ();
		if (t.tObjID == 0)  return;
		MakeObjectCube(t.tObjID, 0);
	}
	else
	{
		if (t.WeaponProjectileBase[t.tNewProjBase].iAtlasWidth > 0)
		{
			weapon_getfreeobject();
			if (t.tObjID == 0)  return;
			if (ObjectExist(t.tObjID))  DeleteObject(t.tObjID);
			WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_CURSOROBJECT);
			MakeObjectPlane(t.tObjID, 25, 25);
			WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_NORMAL);
			RotateObject(t.tObjID, 90, 0, 0);
			float scale = 50.0f;
			ScaleObject(t.tObjID, scale, scale, scale);
			LockVertexDataForLimbCore(t.tObjID, 0, 1);
			//PE: Move it up a bit.
			for (int i = 0; i < GetVertexDataVertexCount(); i++)
			{
				float vertX = GetVertexDataPositionX(i);
				float vertY = GetVertexDataPositionY(i);
				float vertZ = GetVertexDataPositionZ(i);
				SetVertexDataPosition(i, vertX, vertY + (scale*0.25f), vertZ);
			}
			SetVertexDataNormals(0, 0, 1, 0);
			SetVertexDataNormals(1, 0, 1, 0);
			SetVertexDataNormals(2, 0, 1, 0);
			SetVertexDataNormals(3, 0, 1, 0);
			SetVertexDataNormals(4, 0, 1, 0);
			SetVertexDataNormals(5, 0, 1, 0);
			UnlockVertexData();
			SetObjectUVManually(t.tObjID, 0, t.WeaponProjectileBase[t.tProjType].iAtlasWidth, t.WeaponProjectileBase[t.tProjType].iAtlasHeight);
		}
		else
		{
			t.tFileName_s = cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.value_s;
			weapon_loadobject();
			if (t.tObjID == 0)
			{
				t.tResult = 0; return;
			}
		}
	}
	t.WeaponProjectileBase[t.tNewProjBase].baseObj = t.tObjID;

	// load particle trail image ID here when particle loaded
	t.WeaponProjectileBase[t.tNewProjBase].particleImageID = 0;
	if ( t.WeaponProjectileBase[t.tNewProjBase].particleType == 99 )
	{
		t.tFileName_s = cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.WeaponProjectileBase[t.tNewProjBase].particleName;
		t.WeaponProjectileBase[t.tNewProjBase].particleImageID = loadinternalimage ( t.tFileName_s.Get() );
		if ( t.WeaponProjectileBase[t.tNewProjBase].particleImageID == 0 )
		{
			timestampactivity ( 0, "Could not find particle image file specified by particleName" );
			t.WeaponProjectileBase[t.tNewProjBase].particleType = 0;
		}
	}

	// load explosion and smoke image ID here when particle loaded
	t.WeaponProjectileBase[t.tNewProjBase].explosionImageID = 0;
	t.WeaponProjectileBase[t.tNewProjBase].explosionSmokeImageID = -1;
	if ( t.WeaponProjectileBase[t.tNewProjBase].explosionType == 99 )
	{
		// explosion
		t.tFileName_s = cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.WeaponProjectileBase[t.tNewProjBase].explosionName;
		t.WeaponProjectileBase[t.tNewProjBase].explosionImageID = loadinternalimage ( t.tFileName_s.Get() );

		// optional smoke
		t.tFileName_s = cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.WeaponProjectileBase[t.tNewProjBase].explosionSmokeName;
		t.WeaponProjectileBase[t.tNewProjBase].explosionSmokeImageID = loadinternalimage ( t.tFileName_s.Get() );
		if ( t.WeaponProjectileBase[t.tNewProjBase].explosionSmokeImageID == 0 ) 
		{
			// use default smoke if non specified
			t.WeaponProjectileBase[t.tNewProjBase].explosionSmokeImageID = -1;
		}
	}

	// load diffuse texture (if any)
	t.tInField_s = "textureD" ; weapon_readfield ( );
	if ( strlen ( t.value_s.Get() ) > 4 )
	{
		t.tFileName_s=cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.value_s;
		weapon_loadtexture ( );
		if (  t.tImgID  ==  0 ) 
		{
			t.tResult = 0; return;
		}
		t.WeaponProjectileBase[t.tNewProjBase].textureD = t.tImgID;

		//  load normal texture
		t.tInField_s = "textureN" ; weapon_readfield ( );
		t.tFileName_s=cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.value_s;
		weapon_loadtexture ( );
		if (  t.tImgID  ==  0 ) 
		{
			t.tResult = 0; return;
		}
		t.WeaponProjectileBase[t.tNewProjBase].textureN = t.tImgID;

		//  load specular texture
		t.tInField_s = "textureS" ; weapon_readfield ( );
		t.tFileName_s=cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.value_s;
		weapon_loadtexture ( );
		if (  t.tImgID  ==  0 ) 
		{
			t.tResult = 0; return;
		}
		t.WeaponProjectileBase[t.tNewProjBase].textureS = t.tImgID;
		TextureObject ( t.tObjID, 0, t.WeaponProjectileBase[t.tNewProjBase].textureD );
	}
	else
	{
		// no TEXTURED field assumes it to be a PBR with built-in texture references (for PBR set)
		// just need to add the detail map and cube map
		int texlid = loadinternaltextureex("effectbank\\reloaded\\media\\detail_default.dds", 1, t.tfullorhalfdivide);
		TextureObject ( t.tObjID, 7, texlid );
		TextureObject ( t.tObjID, 6, t.terrain.imagestartindex+31 );
	}
	ExcludeOn (  t.tObjID );

	// load shader (if specified)
	t.teffectid=0;
	t.tInField_s = "effect" ; weapon_readfield ( );
	if ( Len(t.value_s.Get())<=2 ) 
	{
		SetObjectEffect ( t.tObjID, g.decaleffectoffset );
	}
	else
	{
		t.tFileName_s=cstr("effectbank\\reloaded\\") + t.value_s;
		t.teffectid=loadinternaleffect(t.tFileName_s.Get());
		SetObjectEffect (  t.tObjID,t.teffectid );
	}
	t.WeaponProjectileBase[t.tNewProjBase].effectid = t.teffectid;

	// apply no Zwrite if projectile flagged it
	if ( t.WeaponProjectileBase[t.tNewProjBase].noZWrite == 1 )
	{
		DisableObjectZWrite ( t.tObjID );
	}
	SetObjectTransparency ( t.tObjID, 6 );
	PositionObject(t.tObjID, 0, -19000, 0);
	SetObjectCollisionOff(t.tObjID);
	HideObject(t.tObjID);
	// load sounds
	t.tInField_s = "sound" ; weapon_readfield(); t.sound_s = t.value_s;
	if (  t.value_s  !=  "" ) 
	{
		t.tFileName_s = cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.value_s ; weapon_loadsound ( );
		t.WeaponProjectileBase[t.tNewProjBase].sound = t.tSndID;
	}
	t.tInField_s = "soundDeath" ; weapon_readfield(); t.sound_s = t.value_s;
	if (  t.value_s  !=  "" ) 
	{
		t.tFileName_s = cstr("gamecore\\projectileTypes\\") + tProjectileFolder_s + t.value_s ; weapon_loadsound ( );
		t.WeaponProjectileBase[t.tNewProjBase].soundDeath = t.tSndID;
	}

	// create a number of projectiles now for the pool
	// new way for sounds, use one per projectile parent base
	int iSndForBaseSound = 0;
	int iSndForBaseDestroy = 0;
	t.tInField_s = "cacheNumber" ; weapon_readfield( ); t.tcount = (int)t.value1_f;
	t.WeaponProjectileBase[t.tNewProjBase].cacheProjectile = t.tcount;
	t.WeaponProjectileBase[t.tNewProjBase].cacheLoaded = false;
	weapon_projectile_setup ( &iSndForBaseSound, &iSndForBaseDestroy ); //Add one so we get the basetype mapping.

	UnDim (  t.fileData_s );

	t.WeaponProjectileBase[t.tNewProjBase].activeFlag = 1;
	t.tResult = t.tNewProjBase;
}

void weapon_projectile_setup ( int* piSndForBaseSound, int* piSndForBaseDestroy )
{
	// sets up an inactive projectile ready for use
	// takes tNewProjBase, returns tResult (0 failed, or ID of projectileBase)
	// find an empty slot to setup this projectile in
	for ( t.tNew = 1 ; t.tNew<=  g.weaponSystem.numProjectiles; t.tNew++ )
	{
		//PE: Expand array when needed.
		if (t.tNew >= g.weaponSystem.numProjectiles - 1) {
			g.weaponSystem.numProjectiles += 10;
			Dim(t.WeaponProjectile, g.weaponSystem.numProjectiles);
		}
		if ( t.WeaponProjectile[t.tNew].baseType  ==  0 ) 
		{
			t.WeaponProjectile[t.tNew].baseType = t.tNewProjBase;

			// if this projectile type has an object, instance it now
			if ( t.WeaponProjectileBase[t.tNewProjBase].baseObj > 0 ) 
			{
				weapon_getfreeobject ( );
				if ( t.tObjID  ==  0 ) 
				{
					t.tResult = 0; return;
				}
				t.WeaponProjectile[t.tNew].obj = t.tObjID;
				CloneObject ( t.tObjID,t.WeaponProjectileBase[t.tNewProjBase].baseObj );
				//LB: strangely projectiles have no textures, needed to call it here
				sObject* pObject = GetObjectData(t.tObjID);
				WickedCall_TextureObject(pObject, NULL);
				SetObjectTransparency ( t.tObjID,6 );
				SetObjectMask ( t.tObjID,1 );
				HideObject ( t.tObjID );
			}

			if (t.WeaponProjectileBase[t.tNewProjBase].WPE_Effect.Len() > 0)
			{
				//PE: Need clone but for now.
				t.WeaponProjectile[t.tNew].WPE_Root = WickedCall_LoadWPE(t.WeaponProjectileBase[t.tNewProjBase].WPE_Effect.Get());
				//iAction = 1 Burst all. 2 = Pause. - 3 = Resume. - 4 = Restart - 5 - visible - 6 = not visible. - 7 = pause emit - 8 = resume emit
				WickedCall_PerformEmitterAction(6, t.WeaponProjectile[t.tNew].WPE_Root);
			}

			// make the tracer object
			weapon_getfreeobject ( );
			if ( t.tObjID  ==  0  )  break;
			// no tracer object for MAX for now
			MakeObjectCube (t.tObjID, 0);
			SetObjectMask ( t.tObjID,1 );
			t.WeaponProjectile[t.tNew].tracerObj = t.tObjID;
			HideObject ( t.tObjID );

			// make the sound effect for sound and death
			if ( *piSndForBaseSound == 0 && t.WeaponProjectileBase[t.tNewProjBase].sound > 0 )
			{
				weapon_getfreesound ( );
				if ( t.tSndID > 0 ) 
				{
					CloneSound ( t.tSndID, t.WeaponProjectileBase[t.tNewProjBase].sound );
					*piSndForBaseSound = t.tSndID;
				}
			}
			if ( *piSndForBaseDestroy == 0 && t.WeaponProjectileBase[t.tNewProjBase].soundDeath > 0 )
			{
				weapon_getfreesound ( );
				if ( t.tSndID > 0 ) 
				{
					CloneSound ( t.tSndID, t.WeaponProjectileBase[t.tNewProjBase].soundDeath );
					*piSndForBaseDestroy = t.tSndID;
				}
			}
			t.WeaponProjectile[t.tNew].sound = *piSndForBaseSound;
			t.WeaponProjectile[t.tNew].soundDeath = *piSndForBaseDestroy;

			// done creating instance,can leave
			t.tResult = t.tNew; return;
		}
	}
	t.tResult = 0;
}

void weapon_projectile_make ( bool bUsingVRForAngle, bool bDoNotAdvanceToAvoidPenetration )
{
	//  creates a projectile in the game. Takes;
	//    tProjectileType which is an index of array WeaponProjectileBase()
	//    tSourceEntity, tTracerFlag
	//    tStartX#, tStartY#, tStartZ#, tAngX#, tAngY#, tAngZ#
	//    gunid,firemode

	//  if third person, scan for character targets and adjust X angle (removed targetting for now)
	t.tlocktargetonobject=0;
	if (  t.playercontrol.thirdperson.enabled == 1 && t.playercontrol.thirdperson.cameralocked == 1 ) 
	{
		t.tttbesta_f=500.0;
		for ( t.ttte = 1 ; t.ttte<=  g.entityelementlist; t.ttte++ )
		{
			if ( t.ttte != t.playercontrol.thirdperson.charactere )
			{
				t.tentid=t.entityelement[t.ttte].bankindex;
				if (  t.entityprofile[t.tentid].ischaracter == 1 ) 
				{
					if (  t.tentid>0 && t.entityelement[t.ttte].health>0 ) 
					{
						t.tdx_f=t.entityelement[t.ttte].x-t.tStartX_f;
						t.tdy_f=t.entityelement[t.ttte].y-t.tStartY_f;
						t.tdz_f=t.entityelement[t.ttte].z-t.tStartZ_f;
						t.tdd_f=Sqrt(abs(t.tdx_f*t.tdx_f)+abs(t.tdy_f*t.tdy_f)+abs(t.tdz_f*t.tdz_f));
						if (  t.tdd_f<1000.0 ) 
						{
							t.tda1_f=WrapValue(atan2deg(t.tdx_f,t.tdz_f));
							t.tda2_f=WrapValue(CameraAngleY(t.terrain.gameplaycamera));
							t.tda_f=t.tda1_f-t.tda2_f;
							if (  t.tda_f<-180  )  t.tda_f = t.tda_f+360;
							if (  t.tda_f>180  )  t.tda_f = t.tda_f-360;
							t.tda_f=abs(t.tda_f);
							if (  t.tda_f <= 10.0 ) 
							{
								if (  t.tda_f<t.tttbesta_f ) 
								{
									t.tttbesta_f=t.tda_f ; t.tlocktargetonobject=t.entityelement[t.ttte].obj;
								}
							}
						}
					}
				}
			}
		}
	}

	//  find a free projectile
	t.tNewProj = 0;
	for ( t.tProj = 1 ; t.tProj<=  g.weaponSystem.numProjectiles; t.tProj++ )
	{
		if (  t.WeaponProjectile[t.tProj].activeFlag  ==  0 ) 
		{
			if (  t.WeaponProjectile[t.tProj].baseType  ==  t.tProjectileType ) 
			{
				t.tNewProj = t.tProj;
				break;
			}
		}
	}
	if (  t.tNewProj  ==  0 ) 
	{
		t.tResult = 0; return;
	}

	t.WeaponProjectile[t.tNewProj].activeFlag = 1;
	t.tTimer = Timer();
	t.WeaponProjectile[t.tNewProj].createStamp = t.tTimer;
	t.WeaponProjectile[t.tNewProj].resultIntervalStamp = t.tTimer;
	t.WeaponProjectile[t.tNewProj].soundIntervalStamp = t.tTimer;
	t.WeaponProjectile[t.tNewProj].sourceEntity = t.tSourceEntity;

	// a player projectile (source 0) records its trigger pull and gun for the player's hit record
	extern int g_iPlayerGunShotCount;
	t.WeaponProjectile[t.tNewProj].iPlayerShot = -1;
	t.WeaponProjectile[t.tNewProj].iPlayerGunID = 0;
	if ( t.tSourceEntity == 0 )
	{
		t.WeaponProjectile[t.tNewProj].iPlayerShot = g_iPlayerGunShotCount;
		t.WeaponProjectile[t.tNewProj].iPlayerGunID = t.gunid;
	}

	//  Settings from weapon properties
	t.WeaponProjectile[t.tNewProj].usespotlighting = g.firemodes[t.gunid][g.firemode].settings.usespotlighting;
	if ( t.WeaponProjectileBase[t.tProjectileType].overridespotlighting != 0 ) 
	{
		t.WeaponProjectile[t.tNewProj].usespotlighting = t.WeaponProjectileBase[t.tProjectileType].overridespotlighting;
	}

	//  adjustment to start position to avoid penetrating
	t.tobj = t.WeaponProjectile[t.tNewProj].tracerObj;
	t.tweaponlimb=t.WeaponProjectileBase[t.tProjectileType].attachToWeaponLimb;
	if (  t.tweaponlimb >= 0 ) 
	{
		//  use start position described by weapon limb
		if (  LimbExist(t.currentgunobj,t.tweaponlimb) == 1 ) 
		{
			t.tStartX_f = LimbPositionX(t.currentgunobj,t.tweaponlimb);
			t.tStartY_f = LimbPositionY(t.currentgunobj,t.tweaponlimb);
			t.tStartZ_f = LimbPositionZ(t.currentgunobj,t.tweaponlimb);
			if (bUsingVRForAngle == false)
			{
				t.tAngX_f = LimbAngleX(t.currentgunobj, t.tweaponlimb);
				t.tAngY_f = LimbAngleY(t.currentgunobj, t.tweaponlimb);
				t.tAngZ_f = LimbAngleZ(t.currentgunobj, t.tweaponlimb);
			}
		}
	}

	//  modify start position
	t.tStartY_f += t.WeaponProjectileBase[t.tProjectileType].vert_f;		

	//  adjustment to start position to avoid penetrating
	PositionObject (  t.tobj,t.tStartX_f,t.tStartY_f,t.tStartZ_f );
	if (t.WeaponProjectile[t.tProj].WPE_Root > 0)
	{
		bool WickedCall_ParticleEffectPosition(uint32_t root, float fX, float fY, float fZ);
		uint32_t root = t.WeaponProjectile[t.tProj].WPE_Root;
		WickedCall_ParticleEffectPosition(root, t.tStartX_f, t.tStartY_f, t.tStartZ_f);
		//iAction = 1 Burst all. 2 = Pause. - 3 = Resume. - 4 = Restart - 5 - visible - 6 = not visible. - 7 = pause emit - 8 = resume emit
		void WickedCall_PerformEmitterAction(int iAction, uint32_t emitter_root);
		WickedCall_PerformEmitterAction(4, root);
		WickedCall_PerformEmitterAction(5, root);
		WickedCall_PerformEmitterAction(8, root);
	}
	//  auto targetting removed in favour of camera based targetting (if camera in locked mode)
	if (  t.playercontrol.thirdperson.enabled == 1 && t.tSourceEntity == 0 ) 
	{
		if (  t.playercontrol.thirdperson.cameralocked == 1 ) 
		{
			if (  t.tlocktargetonobject>0 ) 
			{
				//  camera fixed, targetting help
				t.tcenterobjy_f=ObjectPositionY(t.tlocktargetonobject)+(35.0f);
				PointObject (  t.tobj,ObjectPositionX(t.tlocktargetonobject),t.tcenterobjy_f,ObjectPositionZ(t.tlocktargetonobject) );
				t.tAngX_f=ObjectAngleX(t.tobj);
				t.tAngZ_f=ObjectAngleZ(t.tobj);
			}
			else
			{
				//  camera fixed, no target
				t.tAngX_f=0;
				t.tAngY_f=CameraAngleY(t.terrain.gameplaycamera) ;
				t.tAngZ_f=0;
			}
		}
		else
		{
			//  nonfixed camera, use users camera view
			t.tAngX_f=CameraAngleX(t.terrain.gameplaycamera) ;
			t.tAngY_f=CameraAngleY(t.terrain.gameplaycamera) ;
			t.tAngZ_f=CameraAngleZ(t.terrain.gameplaycamera) ;
			t.tAngX_f=t.tAngX_f-13.0f;
		}
	}
	RotateObject ( t.tobj, t.tAngX_f , t.tAngY_f , t.tAngZ_f );
	float fAvoidPlayerPenetration = t.WeaponProjectileBase[t.tProjectileType].avoidPlayerPenetration_f;
	if (bDoNotAdvanceToAvoidPenetration == true) fAvoidPlayerPenetration = 0;
	MoveObject ( t.tobj, fAvoidPlayerPenetration);
	t.tStartX_f = ObjectPositionX(t.tobj);
	t.tStartY_f = ObjectPositionY(t.tobj);
	t.tStartZ_f = ObjectPositionZ(t.tobj);

	t.WeaponProjectile[t.tNewProj].startXPos_f = t.tStartX_f;
	t.WeaponProjectile[t.tNewProj].startYPos_f = t.tStartY_f;
	t.WeaponProjectile[t.tNewProj].startZPos_f = t.tStartZ_f;
	t.WeaponProjectile[t.tNewProj].xPos_f = t.tStartX_f;
	t.WeaponProjectile[t.tNewProj].yPos_f = t.tStartY_f;
	t.WeaponProjectile[t.tNewProj].zPos_f = t.tStartZ_f;

	//  use the tracer object (which exists for all projectiles) to calculate speeds and angles
	t.WeaponProjectile[t.tNewProj].tracerFlag = t.tTracerFlag;
	t.tobj = t.WeaponProjectile[t.tNewProj].tracerObj;
	PositionObject (  t.tobj,t.tStartX_f,t.tStartY_f,t.tStartZ_f );
	t.tAngRandom = (int)(t.WeaponProjectileBase[t.tProjectileType].xAccuracy_f * 100);
	t.xAngRandom_f = (Rnd(t.tAngRandom)*2 - t.tAngRandom)/100.0f ;
	t.tAngRandom = (int)(t.WeaponProjectileBase[t.tProjectileType].yAccuracy_f * 100);
	t.yAngRandom_f = (Rnd(t.tAngRandom)*2 - t.tAngRandom)/100.0f ;
	RotateObject (  t.tobj,t.tAngX_f + t.xAngRandom_f ,t.tAngY_f + t.yAngRandom_f,t.tAngZ_f );
	t.tSpeedRange = (t.WeaponProjectileBase[t.tProjectileType].speedMax_f - t.WeaponProjectileBase[t.tProjectileType].speedMin_f)*100;
	t.tspeed_f = t.WeaponProjectileBase[t.tProjectileType].speedMin_f + Rnd(t.tSpeedRange)/100.0f;
	MoveObject (  t.tobj,t.tspeed_f );
	t.WeaponProjectile[t.tNewProj].xSpeed_f = ObjectPositionX(t.tobj) - t.tStartX_f;
	t.WeaponProjectile[t.tNewProj].ySpeed_f = ObjectPositionY(t.tobj) - t.tStartY_f;
	t.WeaponProjectile[t.tNewProj].zSpeed_f = ObjectPositionZ(t.tobj) - t.tStartZ_f;
	t.WeaponProjectile[t.tNewProj].xAng_f = ObjectAngleX(t.tobj);
	t.WeaponProjectile[t.tNewProj].yAng_f = ObjectAngleY(t.tobj);
	t.WeaponProjectile[t.tNewProj].zAng_f = ObjectAngleZ(t.tobj);
	t.WeaponProjectile[t.tNewProj].acceleration_f = t.tspeed_f * t.WeaponProjectileBase[t.tProjectileType].thrustModifier_f;
	t.WeaponProjectile[t.tNewProj].fDecalFrame = t.WeaponProjectileBase[t.tProjectileType].fDecalFrame;
	t.WeaponProjectile[t.tNewProj].fDecalSpeed = t.WeaponProjectileBase[t.tProjectileType].fDecalSpeed;
	
	//uint32_t WickedCall_LoadWPE(char* filename)
	
	//  calculate random rotation speeds
	t.tSpeedRange = (int)((t.WeaponProjectileBase[t.tProjectileType].speedAngMaxX_f - t.WeaponProjectileBase[t.tProjectileType].speedAngMinX_f)*100);
	t.WeaponProjectile[t.tNewProj].xAngSpeed_f = t.WeaponProjectileBase[t.tProjectileType].speedAngMinX_f + Rnd(t.tSpeedRange)/100.0;
	t.tSpeedRange = (int)((t.WeaponProjectileBase[t.tProjectileType].speedAngMaxY_f - t.WeaponProjectileBase[t.tProjectileType].speedAngMinY_f)*100);
	t.WeaponProjectile[t.tNewProj].yAngSpeed_f = t.WeaponProjectileBase[t.tProjectileType].speedAngMinY_f + Rnd(t.tSpeedRange)/100.0;
	t.tSpeedRange = (int)((t.WeaponProjectileBase[t.tProjectileType].speedAngMaxZ_f - t.WeaponProjectileBase[t.tProjectileType].speedAngMinZ_f)*100);
	t.WeaponProjectile[t.tNewProj].zAngSpeed_f = t.WeaponProjectileBase[t.tProjectileType].speedAngMinZ_f + Rnd(t.tSpeedRange)/100.0;

	//  calculate random turn speeds
	t.tSpeedRange = (int)((t.WeaponProjectileBase[t.tProjectileType].speedTurnMaxX_f - t.WeaponProjectileBase[t.tProjectileType].speedTurnMinX_f)*100);
	t.WeaponProjectile[t.tNewProj].xTurnSpeed_f = t.WeaponProjectileBase[t.tProjectileType].speedTurnMinX_f + Rnd(t.tSpeedRange)/100.0;
	t.tSpeedRange = (int)((t.WeaponProjectileBase[t.tProjectileType].speedTurnMaxY_f - t.WeaponProjectileBase[t.tProjectileType].speedTurnMinY_f)*100);
	t.WeaponProjectile[t.tNewProj].yTurnSpeed_f = t.WeaponProjectileBase[t.tProjectileType].speedTurnMinY_f + Rnd(t.tSpeedRange)/100.0;
	t.tSpeedRange = (int)((t.WeaponProjectileBase[t.tProjectileType].speedTurnMaxZ_f - t.WeaponProjectileBase[t.tProjectileType].speedTurnMinZ_f)*100);
	t.WeaponProjectile[t.tNewProj].zTurnSpeed_f = t.WeaponProjectileBase[t.tProjectileType].speedTurnMinZ_f + Rnd(t.tSpeedRange)/100.0;
	t.WeaponProjectile[t.tNewProj].xTurn_f = 0;
	t.WeaponProjectile[t.tNewProj].yTurn_f = 0;
	t.WeaponProjectile[t.tNewProj].zTurn_f = 0;

	//  if this projectile has an object, position it now
	t.tobj = t.WeaponProjectile[t.tNewProj].obj;
	if (  t.tobj > 0 ) 
	{
		ShowObject (  t.tobj );
		PositionObject (  t.tobj,t.tStartX_f,t.tStartY_f,t.tStartZ_f );
		RotateObject (  t.tobj,t.WeaponProjectile[t.tNewProj].xAng_f,t.WeaponProjectile[t.tNewProj].yAng_f,t.WeaponProjectile[t.tNewProj].zAng_f );
	}

	//  if this projectile uses real physics, grant this now
	if (  t.tobj > 0 ) 
	{
		if (  t.WeaponProjectileBase[t.tProjectileType].usingRealPhysics>0 ) 
		{
			t.tweight=-1 ; t.tfriction=-1;
			// for the moment, not understanding this logic whicvh seems to rotate the inertia
			// of the projectile -40 degrees off camera forward direction?
			// so, get current projectile position
			float fOldObjectX = ObjectPositionX(t.tobj);
			float fOldObjectY = ObjectPositionY(t.tobj);
			float fOldObjectZ = ObjectPositionZ(t.tobj);
			// orient it to same angle as camera
			if (bUsingVRForAngle == true)
				SetObjectToObjectOrientation ( t.tobj, GGVR_GetLaserGuideObject() );
			else
				SetObjectToCameraOrientation ( t.tobj );
			// move 10 units in that direction
			MoveObject (t.tobj, 10.0);
			// get the normals for the trajectory
			t.WeaponProjectile[t.tNewProj].xSpeed_f = ObjectPositionX(t.tobj) - fOldObjectX;
			t.WeaponProjectile[t.tNewProj].ySpeed_f = ObjectPositionY(t.tobj) - fOldObjectY;
			t.WeaponProjectile[t.tNewProj].zSpeed_f = ObjectPositionZ(t.tobj) - fOldObjectZ;
			// and advance more to avoid clipping player camera
			if (bDoNotAdvanceToAvoidPenetration == true)
			{
				// player facing obstruction, keep in same spot as start pos
				MoveObject (t.tobj, -10.0);
			}
			else
			{
				MoveObject (t.tobj, 20.0);
			}
			ODECreateDynamicBox (  t.tobj,-1,0,t.tweight,(float)t.tfriction,-1 );
			// GG: one that acts on contact ends on its body's first contact (weapon_projectile_loop), not by scene picks
			if ( t.WeaponProjectileBase[t.tProjectileType].resultBounce > 0 )
			{
				int iThrowerObj = 0;
				int iSource = t.WeaponProjectile[t.tNewProj].sourceEntity;
				if ( iSource > 0 ) iThrowerObj = t.entityelement[iSource].obj;
				else if ( t.playercontrol.thirdperson.enabled == 1 && t.playercontrol.thirdperson.charactere > 0 ) iThrowerObj = t.entityelement[t.playercontrol.thirdperson.charactere].obj;
				ODEWatchProjectileContact ( t.tobj, iThrowerObj );
			}
			t.tinertiax_f=t.WeaponProjectile[t.tNewProj].xSpeed_f*t.WeaponProjectile[t.tNewProj].acceleration_f;
			t.tinertiay_f=t.WeaponProjectile[t.tNewProj].ySpeed_f*t.WeaponProjectile[t.tNewProj].acceleration_f;
			t.tinertiaz_f=t.WeaponProjectile[t.tNewProj].zSpeed_f*t.WeaponProjectile[t.tNewProj].acceleration_f;
			// new grenade range massively reduced because of this!
			// will need further investigation as we test Classic vs MAX weapons
			t.tidiv_f = 1.0f;
			ODESetLinearVelocity (  t.tobj,t.tinertiax_f*t.tidiv_f,t.tinertiay_f*t.tidiv_f,t.tinertiaz_f*t.tidiv_f );
			t.tinertiax_f=t.WeaponProjectile[t.tNewProj].xTurnSpeed_f;
			t.tinertiay_f=t.WeaponProjectile[t.tNewProj].yTurnSpeed_f;
			t.tinertiaz_f=t.WeaponProjectile[t.tNewProj].zTurnSpeed_f;
			ODESetAngularVelocity (  t.tobj,t.tinertiax_f,t.tinertiay_f,t.tinertiaz_f );
		}
	}

	//  if this projectile has a sound that loops, start it now
	if (  t.tStartY_f<CameraPositionY(0)+10000 ) 
	{
		t.tSndID = t.WeaponProjectile[t.tNewProj].sound;
		if (  t.tSndID > 0 ) 
		{
			if (  t.WeaponProjectileBase[t.tProjectileType].soundLoopFlag  ==  1 ) 
			{
				if (SoundExist(t.tSndID) == 1)
				{
					PositionSound(t.tSndID, t.tStartX_f, t.tStartY_f, t.tStartZ_f);
					t.tsoundfreq = t.WeaponProjectileBase[t.tProjectileType].soundDopplerBaseSpeed;
					if (t.tsoundfreq < 10000)  t.tsoundfreq = 10000;
					if (t.tsoundfreq > 60000)  t.tsoundfreq = 60000;
					SetSoundSpeed(t.tSndID, t.tsoundfreq);
					LoopSound(t.tSndID);
					if (t.WeaponProjectileBase[t.tProjectileType].soundDopplerFlag == 1)
					{
						t.tProj = t.tNewProj; weapon_projectile_setDistFromPlayer();
					}
				}
			}
		}
	}

	// setup particle emitters for this projectile
	if ( t.WeaponProjectileBase[t.tProjectileType].particleType > 0 ) 
	{
		ravey_particles_get_free_emitter ( );
		t.WeaponProjectile[t.tNewProj].tempEmitter = t.tResult;
		if ( t.tResult>0 ) 
		{
			weapon_add_projectile_particles ( );
		}
	}

	//  returns tResult (0 not made, 1 successful)
	//  returns tProjectile (index of WeaponProjectile() of projectile made)
	t.tResult = 1 ; t.tProjectile = t.tNewProj;
}

void weapon_add_projectile_particles ( void )
{
	//  takes tResult as the id of the partiels and t.tobj as the parent object
	g.tEmitter.id = t.tResult;
	g.tEmitter.emitterLife = 0;
	g.tEmitter.parentObject = t.tobj;
	g.tEmitter.parentLimb = 0;
	g.tEmitter.isAnObjectEmitter = 0;

	// load stock or custom particle trail image
	g.tEmitter.imageNumber = RAVEY_PARTICLES_IMAGETYPE_LIGHTSMOKE + g.particlesimageoffset;
	switch ( t.WeaponProjectileBase[t.tProjectileType].particleType )
	{
		// Stock particle images
		case 1 : g.tEmitter.imageNumber = RAVEY_PARTICLES_IMAGETYPE_LIGHTSMOKE + g.particlesimageoffset; break;
		case 2 : g.tEmitter.imageNumber = RAVEY_PARTICLES_IMAGETYPE_FLARE + g.particlesimageoffset; break;
		case 3 : g.tEmitter.imageNumber = RAVEY_PARTICLES_IMAGETYPE_FLAME + g.particlesimageoffset; break;

		// Custom particle image
		case 99 : 
			g.tEmitter.imageNumber = t.WeaponProjectileBase[t.tProjectileType].particleImageID; 
			if ( g.tEmitter.imageNumber == 0 )
			{
				g.tEmitter.imageNumber = RAVEY_PARTICLES_IMAGETYPE_LIGHTSMOKE + g.particlesimageoffset;
				t.WeaponProjectileBase[t.tProjectileType].particleType = 0;
			}
			break;
	}
	g.tEmitter.isAnimated = 1;
	g.tEmitter.animationSpeed = 1/2.0;
	g.tEmitter.isLooping = 1;
	g.tEmitter.frameCount = 64;
	g.tEmitter.startFrame = 0;
	g.tEmitter.endFrame = 63;
	g.tEmitter.startsOffRandomAngle = 1;
	g.tEmitter.offsetMinX = -2;
	g.tEmitter.offsetMinY = -2;
	g.tEmitter.offsetMinZ = -2;
	g.tEmitter.offsetMaxX = 2;
	g.tEmitter.offsetMaxY = 2;
	g.tEmitter.offsetMaxZ = 2;
	g.tEmitter.scaleStartMin = 8;
	g.tEmitter.scaleStartMax = 12;
	g.tEmitter.scaleEndMin = 50;
	g.tEmitter.scaleEndMax = 60;
	g.tEmitter.movementSpeedMinX = -0.1f;
	g.tEmitter.movementSpeedMinY = -0.1f;
	g.tEmitter.movementSpeedMinZ = -0.1f;
	g.tEmitter.movementSpeedMaxX = 0.1f;
	g.tEmitter.movementSpeedMaxY = 0.1f;
	g.tEmitter.movementSpeedMaxZ = 0.1f;
	g.tEmitter.rotateSpeedMinZ = -0.1f;
	g.tEmitter.rotateSpeedMaxZ = 0.1f;
	g.tEmitter.startGravity = 0;
	g.tEmitter.endGravity = 0;
	g.tEmitter.lifeMin = 1000;
	g.tEmitter.lifeMax = 2000;
	g.tEmitter.alphaStartMin = 40;
	g.tEmitter.alphaStartMax = 75;
	g.tEmitter.alphaEndMin = 0;
	g.tEmitter.alphaEndMax = 0;
	g.tEmitter.frequency = 50;
	ravey_particles_add_emitter ( );
}

void weapon_projectile_reset ( void )
{
	//  for now simply any looping sounds
	for ( t.tProj = 1 ; t.tProj<=  g.weaponSystem.numProjectiles; t.tProj++ )
	{
		if (  t.WeaponProjectile[t.tProj].sound>0 ) 
		{
			if (  SoundExist(t.WeaponProjectile[t.tProj].sound) == 1 ) 
			{
				StopSound (  t.WeaponProjectile[t.tProj].sound );
			}
		}
	}
}

void weapon_mp_projectile_reset ( void )
{
	for ( t.tProj = 1 ; t.tProj<=  g.weaponSystem.numProjectiles; t.tProj++ )
	{
		if (  t.WeaponProjectile[t.tProj].activeFlag  ==  1 ) 
		{
			weapon_projectile_destroy ( );
		}
	}
}

void weapon_projectile_setDistFromPlayer ( void )
{
	//  sets the distance of projectile from player. Takes tProj
	t.txDist_f = CameraPositionX(0) - t.WeaponProjectile[t.tProj].xPos_f;
	t.tyDist_f = CameraPositionY(0) - t.WeaponProjectile[t.tProj].yPos_f;
	t.tzDist_f = CameraPositionZ(0) - t.WeaponProjectile[t.tProj].zPos_f;
	t.WeaponProjectile[t.tProj].soundDistFromPlayer_f = Sqrt(t.txDist_f*t.txDist_f + t.tyDist_f*t.tyDist_f + t.tzDist_f*t.tzDist_f);
}

void weapon_projectile_destroy ( void )
{
	//  resets a projectile back to it's hidden default state. Takes tProj
	t.WeaponProjectile[t.tProj].activeFlag = 0;
	if (  t.WeaponProjectile[t.tProj].obj > 0  )  HideObject (  t.WeaponProjectile[t.tProj].obj );

	//  remove physics object if used
	t.tProjectileType = t.WeaponProjectile[t.tProj].baseType;
	if (  t.WeaponProjectileBase[t.tProjectileType].usingRealPhysics>0 ) 
	{
		ODEDestroyObject (  t.WeaponProjectile[t.tProj].obj );
	}

	//  stop any looping sound
	if (  t.WeaponProjectile[t.tProj].sound > 0 ) 
	{
		StopSound (  t.WeaponProjectile[t.tProj].sound );
	}

	//  temp killing of hardcoded particle emitter
	if (  t.WeaponProjectile[t.tProj].tempEmitter > 0 ) 
	{
		t.tRaveyParticlesEmitterID = t.WeaponProjectile[t.tProj].tempEmitter;
		ravey_particles_delete_emitter ( );
	}

	if (t.WeaponProjectile[t.tProj].WPE_Root > 0)
	{
		//PE: Add a pauseemitter , so no one is added but the rest can just stay and emitter will stop.
		bool WickedCall_ParticleEffectPosition(uint32_t root, float fX, float fY, float fZ);
		uint32_t root = t.WeaponProjectile[t.tProj].WPE_Root;
		//iAction = 1 Burst all. 2 = Pause. - 3 = Resume. - 4 = Restart - 5 - visible - 6 = not visible.
		void WickedCall_PerformEmitterAction(int iAction, uint32_t emitter_root);
		WickedCall_PerformEmitterAction(7, root); //PE: Stop emit new particles. but keep simulation active.

	}

}

void weapon_projectileresult_make (int customdecal )
{
	// creates a projectile result, which is an explosion, or decal, or many other possibilities. Takes;
	// t.tProjectileName_s 
	// tProjectileResult which is value of constant WEAPON_PROJECTILERESULT_*
	// t.tProjectileResultExplosionImageID passed in if a custom projectile explosion used
	// t.tProjectileResultLightFlag 
	// t.tProjectileResultSmokeImageID 
	// t.tProjectileResultSparksCount
	// t.tProjectileResultSize
	// t.tProjectileResultSmokeSize
	// t.tProjectileResultSparksSize
	// tHitObj, tX#, tY#, tZ#, tAngX#, tAngY#, tAngZ#
	// t.tDamage_f, tRadius#, tAICharacter etc.
	// tSoundID, tSourceEntity
	t.tResult = 0;

	// GG: the surface it ended on (weapon_projectile_setresultnormal), taken so any later call starts with none
	GGVECTOR3 vecResultNormal = g_vecProjectileResultNormal;
	bool bEffectOnNormal = g_bProjectileResultOnNormal;
	g_vecProjectileResultNormal = GGVECTOR3 ( 0, 0, 0 );
	g_bProjectileResultOnNormal = false;

	// the player's hit record: an entry for each entity a player projectile's hit or blast damages
	if ( t.tSourceEntity == 0 && t.tProj > 0 && t.tProj <= g.weaponSystem.numProjectiles )
		playerhit_openblast ( t.WeaponProjectile[t.tProj].iPlayerShot, t.WeaponProjectile[t.tProj].iPlayerGunID, t.tx_f, t.ty_f, t.tz_f );
	int iCustomDecal = 0;
	if (customdecal > 0)
		iCustomDecal = customdecal;

	switch ( t.tProjectileResult ) 
	{
		case WEAPON_PROJECTILERESULT_DAMAGE:
		{
			// 080618 - copied from below - trigger direct damage if hit an entity
			if ( t.tHitObj>0 ) 
			{
				for ( t.ttte = 1 ; t.ttte <= g.entityelementlist; t.ttte++ )
				{
					t.entid=t.entityelement[t.ttte].bankindex;
					if (  t.entid>0 ) 
					{
						if (  t.entityelement[t.ttte].obj == t.tHitObj ) 
						{
							t.tdamage=(int)t.tDamage_f;
							t.tdamageforce=(int)t.tDamage_f;
							t.tdamagesource=2;
							t.brayx1_f=CameraPositionX(t.terrain.gameplaycamera);
							t.brayy1_f=CameraPositionY(t.terrain.gameplaycamera);
							t.brayz1_f=CameraPositionZ(t.terrain.gameplaycamera);
							t.brayx2_f=t.tx_f ; t.brayy2_f=t.ty_f ; t.brayz2_f=t.tz_f;
							entity_applydamage ( );
							break;
						}
					}
				}
			}
		}
		break;

		case WEAPON_PROJECTILERESULT_EXPLODE:
		case WEAPON_PROJECTILERESULT_CUSTOM:
		{
			// trigger stock explosion
			t.tXPos_f = t.tx_f ; t.tYPos_f = t.ty_f ; t.tZPos_f = t.tz_f;
			if ( t.tProjectileResult == WEAPON_PROJECTILERESULT_CUSTOM )
			{
				// custom explosion image
				int iLightFlag = t.tProjectileResultLightFlag;
				int iSmokeImageID = t.tProjectileResultSmokeImageID;
				int iSparksCount = t.tProjectileResultSparksCount;
				float fSize = (float)t.tProjectileResultSize / 100.0f;
				float fSmokeSize = (float)t.tProjectileResultSmokeSize / 100.0f;
				float fSparksSize = (float)t.tProjectileResultSparksSize / 100.0f;

				//PE: Scan for custom explosion.
				if (t.tSourceEntity > 0 && t.tSourceEntity < t.entityelement.size())
				{
					cstr explodename = t.entityelement[t.tSourceEntity].eleprof.explodable_decalname;
					cstr lexplodename = explodename.Lower();
					if (lexplodename.Len() > 0)
					{
						for (int i = 1; i <= g.decalmax; i++)
						{
							cstr ldecal = t.decal[i].name_s.Lower();
							if (ldecal == lexplodename)
							{
								iCustomDecal = i;
								break;
							}
						}
					}
				}
				if (iCustomDecal > 0)
				{
					int olddecalrange = g.decalrange;
					g.decalrange = 10000;
					g.decalx = t.tXPos_f;
					g.decaly = t.tYPos_f;
					g.decalz = t.tZPos_f;
					t.decalorient = 0;
					t.decalid = iCustomDecal;
					t.currentdecald = 0;
					decalelement_create();
					g.decalrange = olddecalrange;
					// GG: stood on the surface hit when the projectile's file asks (wpeexplosionnormal)
					if ( bEffectOnNormal && t.currentdecald > 0 ) decalelement_standon ( t.currentdecald, vecResultNormal.x, vecResultNormal.y, vecResultNormal.z );
				}
				else if (t.decalglobal.newexplosion > 0 && t.decal[t.decalglobal.newexplosion].newparticle.bWPE)
				{
					int olddecalrange = g.decalrange;
					g.decalrange = 10000;
					g.decalx = t.tXPos_f;
					g.decaly = t.tYPos_f;
					g.decalz = t.tZPos_f;
					t.decalid = t.decalglobal.newexplosion;
					t.decalorient = 0;
					decalelement_create();
					g.decalrange = olddecalrange;
				}
				else
				{
					explosion_custom(t.tProjectileResultExplosionImageID, iLightFlag, iSmokeImageID, iSparksCount, fSize, fSmokeSize, fSparksSize, (int)t.tXPos_f, (int)t.tYPos_f, (int)t.tZPos_f);
				}
			}
			else
			{
				// stock explosion
				if ( t.tradius_f == 0 ) 
				{
					// fireball if radius is zero
					explosion_fireball(t.tXPos_f,t.tYPos_f,t.tZPos_f);
				}
				else
				{
					// rocket explosion if it has a radius of damage
					explosion_rocket(t.tXPos_f,t.tYPos_f,t.tZPos_f);
				}
			}

			// play explosion sound
			if (t.tSourceEntity > 0 && t.tSourceEntity < t.entityelement.size() && iCustomDecal > 0 && SoundExist(t.entityelement[t.tSourceEntity].soundset6))
			{
				PositionSound(t.entityelement[t.tSourceEntity].soundset6, t.tx_f, t.ty_f, t.tz_f);
				setsoundspeedtunedfor44k(t.entityelement[t.tSourceEntity].soundset6, 38000 + Rnd(8000));
				PlaySound(t.entityelement[t.tSourceEntity].soundset6);
				extern int i_LastExplosionSoundID;
				i_LastExplosionSoundID = t.entityelement[t.tSourceEntity].soundset6;
			}
			else if (t.tSoundID > 0 && SoundExist(t.tSoundID) == 1)
			{
				PositionSound ( t.tSoundID,t.tx_f,t.ty_f,t.tz_f );
				setsoundspeedtunedfor44k ( t.tSoundID,38000+Rnd(8000) );
				PlaySound ( t.tSoundID );
			}

			// record the explosion for any LUA script detection
			if ( strlen ( t.tProjectileName_s.Get() ) > 0 )
			{
				g.projectileEventType_explosion = 1;
				g.projectileEventType_name = t.tProjectileName_s;
				g.projectileEventType_x = (int)t.tXPos_f;
				g.projectileEventType_y = (int)t.tYPos_f;
				g.projectileEventType_z = (int)t.tZPos_f;
				g.projectileEventType_radius = (int)t.tradius_f;
				g.projectileEventType_damage = (int)t.tDamage_f;
				g.projectileEventType_entityhit = 0;
				g_fProjectileEventNormal[0] = vecResultNormal.x;
				g_fProjectileEventNormal[1] = vecResultNormal.y;
				g_fProjectileEventNormal[2] = vecResultNormal.z;
			}

			// trigger direct damage if hit an entity
			if ( t.tHitObj>0 ) 
			{
				for ( t.ttte = 1 ; t.ttte <= g.entityelementlist; t.ttte++ )
				{
					t.entid=t.entityelement[t.ttte].bankindex;
					if ( t.entid>0 ) 
					{
						if ( t.entityelement[t.ttte].obj == t.tHitObj ) 
						{
							t.tdamage=(int)t.tDamage_f;
							t.tdamageforce=(int)t.tDamage_f;
							t.tdamagesource=2;
							t.brayx1_f=CameraPositionX(t.terrain.gameplaycamera);
							t.brayy1_f=CameraPositionY(t.terrain.gameplaycamera);
							t.brayz1_f=CameraPositionZ(t.terrain.gameplaycamera);
							t.brayx2_f=t.tx_f ; t.brayy2_f=t.ty_f ; t.brayz2_f=t.tz_f;
							entity_applydamage ( );
							if ( strlen ( t.tProjectileName_s.Get() ) > 0 )
							{
								g.projectileEventType_entityhit = t.ttte;
							}
							break;
						}
					}
				}
			}

			// trigger physics sphere explosion at this GetPoint (  (does damage and physics force) )
			t.texplodex_f=t.tx_f;
			t.texplodey_f=t.ty_f;
			t.texplodez_f=t.tz_f;
			t.texploderadius_f=t.tradius_f;
			t.texplodesourceEntity=t.tSourceEntity;
			if ( t.game.runasmultiplayer == 1 && g.mp.coop == 1 ) 
			{
				// Check to see if it was the AI that caused the explosion in coop multiplayer
				if ( t.tSourceEntity != 0 )  g.mp.damageWasFromAI  =  1;
			}
			physics_explodesphere ( );
			t.tResult = 1;
		}
		break;
	}
	playerhit_close ( );
}

void weapon_loadsound ( void )
{
	//  loads in the sound tFileName$ and returns tSndID = 0 (failed) or the DBP sound number
	if (  FileExist(t.tFileName_s.Get())  ==  0 ) 
	{
		t.tSndID = 0; return;
	}
	weapon_getfreesound ( );
	if (  t.tSndID  ==  0  )  return;
	Load3DSound (  t.tFileName_s.Get(),t.tSndID );
	if (  SoundExist(t.tSndID)  ==  0 ) 
	{
		t.tSndID = 0; return;
	}
}

void weapon_LUA_addWeapon ( void )
{
}

void weapon_LUA_addProjectileBase ( void )
{
}

void weapon_LUA_makeProjectile ( void )
{
}

void weapon_LUA_firePlayerWeapon ( void )
{
}

void weapon_LUA_reloadPlayerWeapon ( void )
{
}

void weapon_LUA_changePlayerWeapon ( void )
{
}

void weapon_LUA_changePlayerWeaponMode ( void )
{
}

void weapon_LUA_setWeaponROFsingle ( void )
{
}

void weapon_LUA_setWeaponROFauto ( void )
{
}

void weapon_LUA_setWeaponAmmo ( void )
{
}

void weapon_LUA_setWeaponDamageModifier ( void )
{
}

void weapon_LUA_setWeaponRangeModifier ( void )
{
}
