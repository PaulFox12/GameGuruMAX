//#pragma optimize("", off)

//----------------------------------------------------
//--- GAMEGURU - M-Decal
//----------------------------------------------------

// Includes
#include "stdafx.h"
#include "gameguru.h"
#include <map>

// OPTICK Performance
#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

#include "..\..\..\..\Guru-WickedMAX\wickedcalls.h"
#include "WickedEngine.h"
using namespace std;
using namespace wiGraphics;
using namespace wiScene;
using namespace wiECS;

// Externs
extern void newparticle_updateparticleemitter (newparticletype* pParticle, float fScale, float fX, float fY, float fZ, float fRX, float fRY, float fRZ, GGMATRIX* pmatBaseRotation, bool bAutoDelete, int decal_id = -1, const GGVECTOR3* pvecUp = NULL);

// GG: the up a decal element's WPE effect stands on when it fires (decalelement_standon), by element; none is upright
static std::map<int, GGVECTOR3> g_DecalElementUp;

// 
//  Decal Module
// 
// ---------------------------------------------------------------------------
// GGMAX 2.27: decal element pool = PREWARM + GROW instead of 100 eager objects.
//
// decal_init() used to build all g.decalelementmax (=100) elements at startup and HideObject()
// them. Each is a MakeObjectPlane quad, so the scene carried 100 objects + 100 meshes + 100
// materials + 100 transforms that draw NOTHING, on every level, whether a decal is ever placed
// or not — and Scene::Update walks every ECS entity regardless of visibility. The census that
// found this is SWITCHESCAPE_PERF.md §23; it was the largest non-terrain item in the floor.
//
// ★ The original author had already seen the symptom without the cause. Common_part0.cpp:1378:
//   "//PE: For now until we found out why wicked use all that "update" time on non visible
//    objects." — and walked the pool 499 -> 199 -> 100 to compensate. This addresses the cause,
//   so that workaround can eventually be undone rather than deepened.
//
// PREWARM rather than pure lazy, deliberately: decals are allocated at RUNTIME from bullet
// impacts, so a fully lazy pool would move object creation into a firefight. decal_init() is
// called ONCE at startup (Common_part2.cpp:1292), never per level, so the prewarm is paid during
// app start where nothing is running — and normal play then never allocates at all.
// Growth beyond the prewarm happens in batches so a decal BURST (shotgun, explosion) costs a
// bounded number of grow events rather than one per impact.
//
// ⚠ Only the allocator needs to know about unbuilt slots: decal_hide() guards on ObjectExist(),
// and the two per-frame sweeps (M-Decal.cpp control loop, M-Entity_part4.cpp) both gate on
// `active == 1`, which an unbuilt slot can never be. Verified before this change.
// ⚠⚠ GG_DECAL_GROW_PER_FRAME is the load-bearing number, and the first version of this change
// did not have it. Batching grow CALLS is not enough: a dense burst simply calls the allocator
// many times in one frame, so 75 slots were still built in a single frame and the pre-registered
// hitch gate FAILED — worst_ms 9.5 -> 23.9 and one frame over 16.7 ms (§24.2). The tree pool
// already knew this (GG_TREE_POOL_GROW_PER_FRAME, 2.19): on-demand growth needs a PER-FRAME cap,
// not a per-call one. Budget resets in decalelement_control(), which runs every frame.
#define GG_DECAL_GROW_STEP      8   // most slots one allocator call may add
// ★ MEASURED, do not raise casually: a slot build costs ~1.2 ms — expensive for a 6-vertex quad
// because the cost is the DBP CreateNewObject path plus a GPU buffer, not the geometry. At 4/frame
// the gate read worst_ms 8.6 -> 13.4 (+4.8), which breached the +2 ms criterion even though no
// frame passed 16.7 ms. The excess scales linearly with this number, so it is 2. (§24.2)
#define GG_DECAL_GROW_PER_FRAME 2   // hard ceiling on slot builds in any single frame
// GGMAX 2.33: how far out hidden ENTITY MASTERS are parked (M-Entity_part0.cpp, two sites).
// DEFAULT 100000 = the long-standing behaviour, unchanged. setup.ini `masterpark=<units>`.
// Lowering it BOUNDS the corrupt pickup AABB (a clone shares the master's mesh, and the engine
// merges that mesh's ONE-BONE armature world box — parked out here — into the clone's bounds).
// ⚠ It does NOT make the bounds correct; see the long comment at the park site.
// Lives here purely because this file already hosts the setup.ini-driven game knobs.
int g_masterParkUnits = 100000;
int g_decalPrewarm = 24;      // slots built at startup; setup.ini decalprewarm=<N>
int g_decalBuilt   = 0;       // slots 1..g_decalBuilt exist as objects (high-water mark)
int g_decalGrowBudget = 0;    // slot builds still permitted this frame
int g_decalGrowDeferred = 0;  // diagnostic: decals dropped because the budget was spent
void GGSetDecalPrewarm(int n)
{
	if (n < 0) n = 0;
	if (n > 4096) n = 4096;   // clamped again against decalelementmax when applied
	g_decalPrewarm = n;
}

// Build ONE pool element. Body lifted verbatim from the old decal_init loop; uses locals rather
// than the shared t.tobj/t.f so it is safe to call from the middle of the allocator.
static void decal_buildelement ( int f )
{
	int tobj = g.decalelementoffset + f;
	t.decalelement[f].obj = tobj;
	t.decalelement[f].uvgridsize = 0;
	if (  ObjectExist(tobj) == 1  )  DeleteObject (  tobj );
	MakeObjectPlane (tobj, 100, 100);
	SetObjectTransparency (  tobj, 6 );
	SetObjectCollisionOff (  tobj );
	DisableObjectZWrite (  tobj );
	SetObjectTextureMode (  tobj,2,0 );
	SetObjectLight (  tobj,0 );
	SetObjectCull (  tobj,0 );
	HideObject (  tobj );
	sObject* pObject = GetObjectData(tobj);
	WickedCall_SetObjectCastShadows(pObject, false);
	WickedCall_SetObjectLightToUnlit(pObject, (int)wiScene::MaterialComponent::SHADERTYPE_UNLIT);

	// full image UVs, animation frames are set through the material (decal_setelementuvframe)
	SetObjectUVManually ( tobj, 0, 1, 1 );
}

// set decal animation frame via material UV scale and offset (no mesh rebuild)
void decal_setelementuvframe ( int iObj, int iFrameIndex, int iAcross, int iDown )
{
	if ( iAcross < 1 ) iAcross = 1;
	if ( iDown < 1 ) iDown = 1;
	float fUSize = 1.0f / (float)iAcross;
	float fVSize = 1.0f / (float)iDown;
	float fU = 0.0f;
	float fV = 0.0f;
	if ( iFrameIndex != 0 )
	{
		fU = (iFrameIndex % iAcross) * fUSize;
		fV = (iFrameIndex / iAcross) * fVSize;
	}
	sObject* pObject = GetObjectData ( iObj );
	if ( pObject ) WickedCall_SetObjectTextureUV ( pObject, fUSize, fVSize, fU, fV );
}

// Raise the built high-water mark to `want`, clamped to the pool ceiling. Slot indices are
// 1..decalelementmax-1 because the allocator's own `t.d < g.decalelementmax` test has always
// excluded the last slot — preserved rather than "fixed" so allocation behaviour is unchanged.
void decal_growto ( int want )
{
	int ceiling = g.decalelementmax - 1;
	if ( want > ceiling ) want = ceiling;
	for ( int f = g_decalBuilt + 1 ; f <= want ; f++ ) decal_buildelement ( f );
	if ( want > g_decalBuilt ) g_decalBuilt = want;
}

// Growth on the RUNTIME path, spread across frames. Returns how many slots were actually added.
// The prewarm path calls decal_growto directly and is deliberately NOT budgeted: it runs once at
// app startup where nothing is animating.
static int decal_growbudgeted ( int want )
{
	// ⚠⚠ SELF-REFILLING off the device frame counter, deliberately. The first version refilled
	// the budget in decalelement_control() — but that tick does NOT run in every app state, and
	// in the editor it never ran at all, so the budget sat at 0 and the pool could never grow.
	// The gate then reported "no hitch" while measuring a growth path that never executed
	// (§24.2). A budget that silently disables the feature when some other function stops
	// ticking is the wrong shape: decals would just stop appearing. Keyed on the frame instead,
	// it is correct wherever the allocator is called from.
	static unsigned long long s_lastFrame = ~0ull;
	wi::graphics::GraphicsDevice* dev = wi::graphics::GetDevice();
	const unsigned long long nowFrame = ( dev != nullptr ) ? dev->GetFrameCount() : ( s_lastFrame + 1 );
	if ( nowFrame != s_lastFrame )
	{
		s_lastFrame = nowFrame;
		g_decalGrowBudget = GG_DECAL_GROW_PER_FRAME;
	}
	if ( g_decalGrowBudget <= 0 ) return 0;
	int room = g_decalBuilt + g_decalGrowBudget;
	if ( want > room ) want = room;
	int before = g_decalBuilt;
	decal_growto ( want );
	int added = g_decalBuilt - before;
	g_decalGrowBudget -= added;
	return added;
}

void decal_hide ( void )
{
	for (t.f = 1; t.f <= g.decalelementmax; t.f++)
	{
		t.tobj = g.decalelementoffset + t.f;
		if (ObjectExist(t.tobj) == 1)
		{
			HideObject(t.tobj);
		}
	}

}
void decal_init ( void )
{
	WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_CURSOROBJECT);

	//  Load in all decal
	decal_scaninallref ( );

	//  Precreate elements as each is unique (UV writing)
	//  GGMAX 2.27: only the PREWARM slice, the rest grow on demand (see decal_growto above).
	//  Every remaining .obj must still be stamped so any code reading decalelement[f].obj on an
	//  unbuilt slot gets the id it would eventually have rather than a stale 0.
	for ( t.f = 1 ; t.f <= g.decalelementmax ; t.f++ )
	{
		t.decalelement[t.f].obj = g.decalelementoffset + t.f;
		t.decalelement[t.f].uvgridsize = 0;
	}
	g_decalBuilt = 0;
	decal_growto ( g_decalPrewarm );

	//  ensure fixed decals available
	t.decalglobal.splashdecalrippleid=0;
	t.decalglobal.splashdecalsmallid=0;
	t.decalglobal.splashdecallargeid=0;
	t.decalglobal.splashdecalmistyid=0;
	t.decalglobal.splashdecaldropletsid=0;
	t.decalglobal.splashdecalfoamid=0;
	t.decalglobal.dustflumeid=0;
	t.decalglobal.impactid=0;
	t.decalglobal.bloodsplatid=0;
	t.decalglobal.newexplosion=0;
	int numdecals = 9;
	numdecals = 10;
	for ( t.tdscan = 1 ; t.tdscan<= numdecals; t.tdscan++ )
	{
		if (  t.tdscan == 1  )  t.decal_s = "splash_ripple";
		if (  t.tdscan == 2  )  t.decal_s = "splash_small";
		if (  t.tdscan == 3  )  t.decal_s = "splash_large";
		if (  t.tdscan == 4  )  t.decal_s = "splash_misty";
		if (  t.tdscan == 5  )  t.decal_s = "splash_droplets";
		if (  t.tdscan == 6  )  t.decal_s = "splash_foam";
		if (  t.tdscan == 7  )  t.decal_s = "dustflume";
		if (  t.tdscan == 8  )  t.decal_s = "impact";
		#ifdef DETECTANDUSENEWPARTICLEDECALS
		 if (  t.tdscan == 9  ) t.decal_s = "blood";
		 if (  t.tdscan == 10 ) t.decal_s = "explosion";
		#else
		 if (  t.tdscan == 9  )  t.decal_s = "bloodsplat";
		#endif
		decal_find ( );
		if (  t.decalid>0 ) 
		{
			if (  t.tdscan == 1  )  t.decalglobal.splashdecalrippleid = t.decalid;
			if (  t.tdscan == 2  )  t.decalglobal.splashdecalsmallid = t.decalid;
			if (  t.tdscan == 3  )  t.decalglobal.splashdecallargeid = t.decalid;
			if (  t.tdscan == 4  )  t.decalglobal.splashdecalmistyid = t.decalid;
			if (  t.tdscan == 5  )  t.decalglobal.splashdecaldropletsid = t.decalid;
			if (  t.tdscan == 6  )  t.decalglobal.splashdecalfoamid = t.decalid;
			if (  t.tdscan == 7  )  t.decalglobal.dustflumeid = t.decalid;
			if (  t.tdscan == 8  )  t.decalglobal.impactid = t.decalid;
			if (  t.tdscan == 9  )  t.decalglobal.bloodsplatid = t.decalid;
			if (  t.tdscan == 10 )  t.decalglobal.newexplosion = t.decalid;
		}
	}

	WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_NORMAL);
}

void decal_loaddata ( void )
{
	//  establish some defaults
	t.decal[t.decalid].across                  = 4;
	t.decal[t.decalid].down                    = 4;
	t.decal[t.decalid].variants                = 1;
	t.decal[t.decalid].playspeed_f              = 1.0;
	t.decal[t.decalid].particle.offsety        = 0;
	t.decal[t.decalid].particle.scale          = 100;
	t.decal[t.decalid].particle.randomstartx   = 0;
	t.decal[t.decalid].particle.randomstarty   = 0;
	t.decal[t.decalid].particle.randomstartz   = 0;
	t.decal[t.decalid].particle.linearmotionx  = 0;
	t.decal[t.decalid].particle.linearmotiony  = 0;
	t.decal[t.decalid].particle.linearmotionz  = 0;
	t.decal[t.decalid].particle.lineargravity  = 0;
	t.decal[t.decalid].particle.randommotionx  = 0;
	t.decal[t.decalid].particle.randommotiony  = 0;
	t.decal[t.decalid].particle.randommotionz  = 0;
	t.decal[t.decalid].particle.mirrormode     = 0;
	t.decal[t.decalid].particle.camerazshift   = 500;
	t.decal[t.decalid].particle.scaleonlyx     = 100;
	t.decal[t.decalid].particle.lifeincrement  = 100;
	t.decal[t.decalid].particle.alphaintensity = 100;
	t.decal[t.decalid].particle.animated       = 0;

	//  load decal description
	Dim (  t.data_s,100  );
	t.filename_s = "";
	t.filename_s=t.filename_s+"gamecore\\decals\\"+t.decal_s+"\\decalspec.txt";
	LoadArray (  t.filename_s.Get() , t.data_s );
	for ( t.l = 0 ; t.l<=  99; t.l++ )
	{
		t.line_s=t.data_s[t.l];
		if (  Len(t.line_s.Get())>0 ) 
		{
			if ( strcmp ( Left(t.line_s.Get(),1) , ";" ) != 0 ) 
			{

				//  take fieldname and value
				for ( t.c = 0 ; t.c < Len(t.line_s.Get()); t.c++ )
				{
					if (  t.line_s.Get()[t.c] == '=' ) { t.mid = t.c+1  ; break; }
				}
				t.field_s=Lower(removeedgespaces(Left(t.line_s.Get(),t.mid-1)));
				t.value_s=removeedgespaces(Right(t.line_s.Get(),Len(t.line_s.Get())-t.mid));

				//  take value 1 and 2 from value
				for ( t.c = 0 ; t.c <  Len(t.value_s.Get()); t.c++ )
				{
					if (  t.value_s.Get()[t.c] == ',' ) {  t.mid = t.c+1  ; break; }
				}
				t.value1=ValF(removeedgespaces(Left(t.value_s.Get(),t.mid-1)));
				t.value2=ValF(removeedgespaces(Right(t.value_s.Get(),Len(t.value_s.Get())-t.mid)));

				//  decal
				if (  t.field_s == "across"  )  t.decal[t.decalid].across = t.value1;
				if (  t.field_s == "down"  )  t.decal[t.decalid].down = t.value1;
				if (  t.field_s == "variants"  )  t.decal[t.decalid].variants = t.value1;
				if (  t.field_s == "playspeed"  )  t.decal[t.decalid].playspeed_f = (t.value1+0.0)/100.0;

				//  additional fields for decal based particle function
				if (  t.field_s == "offsety"  )  t.decal[t.decalid].particle.offsety = t.value1;
				if (  t.field_s == "scale"  )  t.decal[t.decalid].particle.scale = t.value1;
				if (  t.field_s == "randomstartx"  )  t.decal[t.decalid].particle.randomstartx = t.value1;
				if (  t.field_s == "randomstarty"  )  t.decal[t.decalid].particle.randomstarty = t.value1;
				if (  t.field_s == "randomstartz"  )  t.decal[t.decalid].particle.randomstartz = t.value1;
				if (  t.field_s == "linearmotionx"  )  t.decal[t.decalid].particle.linearmotionx = t.value1;
				if (  t.field_s == "linearmotiony"  )  t.decal[t.decalid].particle.linearmotiony = t.value1;
				if (  t.field_s == "linearmotionz"  )  t.decal[t.decalid].particle.linearmotionz = t.value1;
				if (  t.field_s == "lineargravity"  )  t.decal[t.decalid].particle.lineargravity = t.value1;
				if (  t.field_s == "randommotionx"  )  t.decal[t.decalid].particle.randommotionx = t.value1;
				if (  t.field_s == "randommotiony"  )  t.decal[t.decalid].particle.randommotiony = t.value1;
				if (  t.field_s == "randommotionz"  )  t.decal[t.decalid].particle.randommotionz = t.value1;
				if (  t.field_s == "mirrormode"  )  t.decal[t.decalid].particle.mirrormode = t.value1;
				if (  t.field_s == "camerazshift"  )  t.decal[t.decalid].particle.camerazshift = t.value1;
				if (  t.field_s == "scaleonlyx"  )  t.decal[t.decalid].particle.scaleonlyx = t.value1;
				if (  t.field_s == "lifeincrement"  )  t.decal[t.decalid].particle.lifeincrement = t.value1;
				if (  t.field_s == "alphaintensity"  )  t.decal[t.decalid].particle.alphaintensity = t.value1;
				if (  t.field_s == "animated"  )  t.decal[t.decalid].particle.animated = t.value1;

			}
		}
	}
	UnDim (  t.data_s );

	//  default scale is 100
	if (  t.decal[t.decalid].particle.scale == 0  )  t.decal[t.decalid].particle.scale = 100;

	//  Some pre-game calculations
	t.decal[t.decalid].framemax=t.decal[t.decalid].across*t.decal[t.decalid].down;
}

void decal_load(void)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	// Load decal data
	decal_loaddata();

	// Load decal image and store name in bank
	t.strwork = ""; t.strwork = t.strwork + "gamecore\\decals\\" + t.decal_s + "\\decal.dds";
	loaddecal(t.strwork.Get(), t.decalid);

	t.decal[t.decalid].newparticle.bWPE = false;
	t.strwork = ""; t.strwork = t.strwork + "gamecore\\decals\\" + t.decal_s + "\\wpe.pe";
	t.decal[t.decalid].newparticle.emitterid = -1;
	t.decal[t.decalid].newparticle.emittername = t.strwork.Get();

	char pAbsPathToParticle[MAX_PATH];
	strcpy(pAbsPathToParticle, t.decal[t.decalid].newparticle.emittername.Get());

	bool bNotInStandalone = false;
	int get_gameisexe(void);
	if (get_gameisexe() != 0)
	{
		//PE: in standalone only check standalone folder , dont use from docwrite.
		DWORD attrib = GetFileAttributesA(pAbsPathToParticle);
		if (attrib == INVALID_FILE_ATTRIBUTES || attrib == FILE_ATTRIBUTE_DIRECTORY)
		{
			bNotInStandalone = true;
		}
	}
	else
	{
		GG_GetRealPath(pAbsPathToParticle, 0);
	}
	if (!bNotInStandalone && FileExist(pAbsPathToParticle) == 1)
	{
		// this load creates a large delay each time test game is used, so allow user to skip this new effect for quicker testing
		extern bool g_bTemporarilyDisableFullDecalEffectLoading;
		if ( g_bTemporarilyDisableFullDecalEffectLoading == false && g.gdisablefulldecaleffects == 0 )
		{
			t.decal[t.decalid].newparticle.iParticle_Floor_Active = 1;
			t.decal[t.decalid].newparticle.bParticle_Show_At_Start = false;
			t.decal[t.decalid].newparticle.bParticle_Looping_Animation = false;
			t.decal[t.decalid].newparticle.bWPE = true;
			//PE: Preload some effects.
			bool preload_wicked_particle_effect(newparticletype * pParticle, int decal_id);
			if (!preload_wicked_particle_effect(&t.decal[t.decalid].newparticle, t.decalid))
			{
				//PE: Bad emitter. continue without WPE.
				char debug[MAX_PATH];
				sprintf(debug, "WPE Error Non Burst Decal: - %s", pAbsPathToParticle);
				timestampactivity(0, debug);
				t.decal[t.decalid].newparticle.bWPE = false;
			}
		}
	}
	if (!t.decal[t.decalid].newparticle.bWPE)
	{
		// Detect and load any new particle associated with this decal
		#ifdef DETECTANDUSENEWPARTICLEDECALS
		t.strwork = ""; t.strwork = t.strwork + "gamecore\\decals\\" + t.decal_s + "\\newparticle";
		t.decal[t.decalid].newparticle.emitterid = -1;
		t.decal[t.decalid].newparticle.emittername = t.strwork.Get();
		strcpy(pAbsPathToParticle, t.decal[t.decalid].newparticle.emittername.Get());
		strcat(pAbsPathToParticle, ".arx");
		GG_GetRealPath(pAbsPathToParticle, 0);
		if (FileExist(pAbsPathToParticle) == 1)
		{
			GGMATRIX* pmatBaseRotation = NULL;
			float fScale = -80.0f;
			float fX = 0.0f;
			float fY = 0.0f;
			float fZ = 0.0f;
			float fRX = 0.0f;
			float fRY = 0.0f;
			float fRZ = 0.0f;
			t.decal[t.decalid].newparticle.iParticle_Floor_Active = 1;
			t.decal[t.decalid].newparticle.bParticle_Show_At_Start = true;
			t.decal[t.decalid].newparticle.bParticle_Looping_Animation = true;
			newparticle_updateparticleemitter(&t.decal[t.decalid].newparticle, fScale, fX, fY, fZ, fRX, fRY, fRZ, pmatBaseRotation, true, t.decalid); // pre-load!
			t.decal[t.decalid].newparticle.bParticle_Show_At_Start = false;
			t.decal[t.decalid].newparticle.bParticle_Looping_Animation = false;
			newparticle_updateparticleemitter(&t.decal[t.decalid].newparticle, fScale, fX, fY, fZ, fRX, fRY, fRZ, pmatBaseRotation, true, t.decalid); // update it!
		}
		#endif
	}
}

void decal_scaninallref ( void )
{
	t.decalid = 1;
	cstr pRootDir = GetDir();
	for (int iDecalsInTwoFolders = 0; iDecalsInTwoFolders < 2; iDecalsInTwoFolders++)
	{
		char pathToUse[MAX_PATH];
		if (iDecalsInTwoFolders == 0)
		{
			// root folder - already here
			strcpy(pathToUse, "");
		}
		if (iDecalsInTwoFolders == 1)
		{
			// writable folder - switch to it
			strcpy(pathToUse, pRootDir.Get());
			GG_GetRealPath(pathToUse, 0);
			SetDir(pathToUse);
		}
		if (PathExist("gamecore") == 1)
		{
			SetDir ("gamecore");
			UnDim (t.filelist_s);
			buildfilelist("decals", "");
			SetDir ("..");
			if (ArrayCount(t.filelist_s) > 0)
			{
				for (t.chkfile = 0; t.chkfile <= ArrayCount(t.filelist_s); t.chkfile++)
				{
					t.file_s = t.filelist_s[t.chkfile];
					if (t.file_s != "." && t.file_s != "..")
					{
						t.tryfile_s = Lower(Right(t.file_s.Get(), 13));
						if (t.tryfile_s == "decalspec.txt")
						{
							t.newdecal_s = Left(t.file_s.Get(), Len(t.file_s.Get()) - 14);
							for (t.tdecalid = 1; t.tdecalid <= g.decalmax; t.tdecalid++)
							{
								if (t.decal[t.tdecalid].name_s == t.newdecal_s)  break;
							}
							if (t.tdecalid > g.decalmax)
							{
								if (t.decalid > g.decalmax)
								{
									g.decalmax = t.decalid;
									Dim (t.decal, g.decalmax);
								}
								t.decal[t.decalid].name_s = t.newdecal_s;
								++t.decalid;
							}
						}
					}
				}
			}
		}
	}

	// and restore root folder
	SetDir(pRootDir.Get());

	// report total number of decals
	g.decalmax = t.decalid - 1;
	t.strwork = ""; t.strwork = t.strwork + "total decals=" + Str(g.decalmax);
	timestampactivity(0, t.strwork.Get());
}

void decal_scaninall_findnewlyaddedgun (void)
{
	// store original gunid
	int storedecalid = t.decalid;

	// Scan entire decals folder (again)
	cstr pRootDir = GetDir();
	for (int iDecalsInTwoFolders = 0; iDecalsInTwoFolders < 2; iDecalsInTwoFolders++)
	{
		char pathToUse[MAX_PATH];
		if (iDecalsInTwoFolders == 0)
		{
			// root folder - already here
			strcpy(pathToUse, "");
		}
		if (iDecalsInTwoFolders == 1)
		{
			// writable folder - switch to it
			strcpy(pathToUse, pRootDir.Get());
			GG_GetRealPath(pathToUse, 0);
			SetDir(pathToUse);
		}
		if (PathExist("gamecore") == 1)
		{
			SetDir ("gamecore");
			UnDim (t.filelist_s);
			buildfilelist("decals", "");
			SetDir ("..");
			if (ArrayCount(t.filelist_s) > 0)
			{
				for (t.chkfile = 0; t.chkfile <= ArrayCount(t.filelist_s); t.chkfile++)
				{
					t.file_s = t.filelist_s[t.chkfile];
					if (t.file_s != "." && t.file_s != "..")
					{
						t.tryfile_s = Lower(Right(t.file_s.Get(), 13));
						if (t.tryfile_s == "decalspec.txt")
						{
							t.newdecal_s = Left(t.file_s.Get(), Len(t.file_s.Get()) - 14);
							for (t.tdecalid = 1; t.tdecalid <= g.decalmax; t.tdecalid++)
							{
								if (t.decal[t.tdecalid].name_s == t.newdecal_s)  break;
							}
							if (t.tdecalid > g.decalmax)
							{
								++g.decalmax;
								if (g.decalmax >= g.decalelementmax)  g.decalmax = g.decalelementmax - 1;
								Dim (t.decal, g.decalmax);
								t.decal[g.decalmax].path_s = pathToUse;
								t.decal[g.decalmax].name_s = t.newdecal_s;
								t.decalid = g.decalmax;
								t.decal_s = t.newdecal_s;
								decal_loaddata ();
								t.strwork = ""; t.strwork = t.strwork + "newly added decal " + Str(t.decalid) + ":" + t.file_s;
								timestampactivity(0, t.strwork.Get());
							}
						}
					}
				}
			}
		}
	}

	// and restore root folder
	SetDir(pRootDir.Get());

	// report total number
	t.strwork = ""; t.strwork = t.strwork + "new total decals=" + Str(g.decalmax);
	timestampactivity(0, t.strwork.Get());

	// restore original
	t.decalid = storedecalid;
}

void decal_loadonlyactivedecals ( void )
{
	//  Load all decals that have been activated
	for ( t.decalid = 1 ; t.decalid<=  g.decalmax; t.decalid++ )
	{
		if (  t.decal[t.decalid].active == 1 ) 
		{
			t.decal_s=t.decal[t.decalid].name_s;
			char pProgressPrompt[256];
			sprintf(pProgressPrompt, "Loading decal %d of %d : %s", t.decalid, g.decalmax, t.decal_s.Get());
			if (t.game.gameisexe == 0)  printscreenprompt(pProgressPrompt); else loadingpageprogress(2);
			decal_load ( );
		}
	}
}

void decal_getparticlefile ( void )
{
	//  if not scanned decal folder, do this now (before we can retrieve particle data)
	if ( t.decal[1].name_s == "" ) decal_scaninallref ( );

	//  takes particlefile$, fill gotparticle with data
	for ( t.decalid = 1 ; t.decalid<=  g.decalmax; t.decalid++ )
	{
		if (  cstr(Lower(t.decal[t.decalid].name_s.Get())) == Lower(t.particlefile_s.Get()) ) 
		{
			if (  t.decal[t.decalid].across == 0 ) 
			{
				t.decal[t.decalid].active=1;
				decal_loaddata ( );
			}
			g.gotparticle=t.decal[t.decalid].particle;
			break;
		}
	}
}

int g_iBlendMode = BLENDMODE_ALPHA;
// GG: a decal element's WPE effect, when it fires, stands on this up (its local Y), not upright (a projectile's explosion
// on the surface it hit); a zero up leaves it upright
void decalelement_standon ( int d, float fNX, float fNY, float fNZ )
{
	if ( d <= 0 || d > g.decalelementmax ) return;
	if ( fNX == 0.0f && fNY == 0.0f && fNZ == 0.0f ) { g_DecalElementUp.erase ( d ); return; }
	g_DecalElementUp[d] = GGVECTOR3 ( fNX, fNY, fNZ );
}

void decalelement_create ( void )
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	WickedCallProbeScope probe(WICKEDCALL_PROBE_DECAL_CREATE);

	//  Decal Orient Modes
	//  0 - perfectly upright facing camera (X=Z=0)
	//  1 - rotated to decalorient x y z#
	//  2 - flattened on Floor (90,0,0)
	//  3 - decal facing camera
	//  4 - decal facing camera (advanced 5.0 toward camera)
	//  5 - decal facing camera (advanced 15.0 toward camera)
	//  7 - decal based particle moves in 3D space (animated image)
	//  8 - decal based particle moves in 3D space (single image) (rotates Z~360)
	//  11 - single image blast then fade out over short time (muzzle from characters) (rotates Z~360)
	//  12 - same as [3] but fades alpha throughout frame life
	//  13 - same as [8] but replaces decal texture with blood splat from HUD

	//  early out if decal over 1000 units away from camera
	t.tdxx=g.decalx-CameraPositionX(t.terrain.gameplaycamera);
	t.tdyy=g.decaly-CameraPositionY(t.terrain.gameplaycamera);
	t.tdzz=g.decalz-CameraPositionZ(t.terrain.gameplaycamera);
	t.tddd=Sqrt(abs(t.tdxx*t.tdxx)+abs(t.tdyy*t.tdyy)+abs(t.tdzz*t.tdzz));

	//  limit decal usage to X distance
	if (  g.decalrange == 0  )  g.decalrange = 1000;
	if (  t.tddd>g.decalrange  )  return;

	//  find free decal element
	//  GGMAX 2.27: search only the BUILT slice, then grow a batch if it is full. Behaviour is
	//  preserved on both ends: the random start (spreads reuse so the same quad is not recycled
	//  every time) is kept but scaled to the built range, and when the pool is genuinely
	//  exhausted t.d is left >= decalelementmax so the existing test below drops the decal
	//  exactly as it does today.
	{
		const int built = g_decalBuilt;
		int spread = built - 1; if ( spread > 50 ) spread = 50; if ( spread < 0 ) spread = 0;
		t.d = 1 + Rnd(spread);
		for (; t.d <= built; t.d++)
		{
			if ( t.decalelement[t.d].active == 0 )  break;
		}
		if ( t.d > built )
		{
			//  No free built slot — grow, but only within this frame's budget. If the budget is
			//  already spent, DROP this decal (t.d left >= decalelementmax) exactly as the
			//  pre-2.27 code did when all slots were busy. Dropping the odd decal during an
			//  extreme burst is self-correcting within a few frames; a 24 ms stall is not.
			if ( decal_growbudgeted ( built + GG_DECAL_GROW_STEP ) > 0 )
				t.d = built + 1;
			else
			{
				g_decalGrowDeferred++;
				t.d = g.decalelementmax;
			}
		}
	}
	if ( t.d < g.decalelementmax )
	{
		bool bReuse = false;
		if (t.decalelement[t.d].decalid == t.decalid && t.decalelement[t.d].newparticle.emitterid > 0)
		{
			bReuse = true;
		}
		// activate new decal element
		t.currentdecald=t.d;
		g_DecalElementUp.erase ( t.d );
		t.decalelement[t.d].decalid=t.decalid;
		t.decalelement[t.d].active=1;
		t.decalelement[t.d].xpos=g.decalx;
		t.decalelement[t.d].ypos=g.decaly;
		t.decalelement[t.d].zpos=g.decalz;
		t.decalelement[t.d].burstloop=t.decalburstloop;
		t.decalelement[t.d].decalforward=t.decalforward;
		t.decalelement[t.d].orient=t.decalorient;
		t.decalelement[t.d].originator=t.originatore;
		t.decalelement[t.d].originatorobj=t.originatorobj;

		if (t.decal[t.decalid].newparticle.bWPE)
		{
			// new particle - uses new particle system
			int iEmitterID = -2;
			if (bReuse) //PE: Reuse already loaded effect.
				iEmitterID = t.decalelement[t.d].newparticle.emitterid;

			t.decalelement[t.d].newparticle = t.decal[t.decalid].newparticle;
			t.decalelement[t.d].newparticle.bParticle_Show_At_Start = true;
			t.decalelement[t.d].newparticle.bParticle_Fire = true;

			t.decalelement[t.d].newparticle.emitterid = iEmitterID;

			// orientation support for some new partidle effects
			// always face camera using Y angle only
			t.decalelement[t.d].newparticle.bParticle_LocalRot_Used = true;
			float fCorrectAngleForParticlesToSpray = GGToDegree(atan2(t.tdxx, t.tdzz)); // angle from camera to point we hit
			fCorrectAngleForParticlesToSpray += 90; // but rotate 90 degrees as FOUNTAIN_DIRECTION pours out to the RIGHT, so need to pour at us (-Z)
			t.decalelement[t.d].newparticle.bParticle_LocalRot_Y = fCorrectAngleForParticlesToSpray;

			// no object required
			t.tobj = 0;
		}
		else
		// legacy decal or new particle
		if (t.decal[t.decalid].newparticle.emitterid != -1)
		{
			// new particle - uses new particle system
			t.decalelement[t.d].newparticle = t.decal[t.decalid].newparticle;
			t.decalelement[t.d].newparticle.bParticle_Show_At_Start = true;
			t.decalelement[t.d].newparticle.bParticle_Fire = true;

			// force a new particle to be loaded (can we simply create a new particle from data copied from the original? speed)
			t.decalelement[t.d].newparticle.emitterid = -2;

			// orientation support for some new partidle effects
			// always face camera using Y angle only
			t.decalelement[t.d].newparticle.bParticle_LocalRot_Used = true;
			float fCorrectAngleForParticlesToSpray = GGToDegree(atan2(t.tdxx, t.tdzz)); // angle from camera to point we hit
			fCorrectAngleForParticlesToSpray += 90; // but rotate 90 degrees as FOUNTAIN_DIRECTION pours out to the RIGHT, so need to pour at us (-Z)
			t.decalelement[t.d].newparticle.bParticle_LocalRot_Y = fCorrectAngleForParticlesToSpray;

			// no object required
			t.tobj = 0;
		}
		else
		{
			// legacy decal
			t.decalelement[t.d].particle = t.decal[t.decalid].particle;

			// frame handling
			if (t.decal[t.decalid].variants > 1)
			{
				t.tvariantsection = t.decal[t.decalid].framemax / t.decal[t.decalid].variants;
				t.tvarchoice = Rnd(t.decal[t.decalid].variants - 1);
				t.decalelement[t.d].frame = t.tvarchoice * t.tvariantsection;
				t.decalelement[t.d].framefinish = ((t.tvarchoice + 1) * t.tvariantsection);
			}
			else
			{
				t.decalelement[t.d].frame = 0;
				t.decalelement[t.d].framefinish = t.decal[t.decalid].framemax;
			}

			//  scale affects
			if (t.decalscalemodx != 0)
			{
				t.decalelement[t.d].particle.randomstartx = (t.decalelement[t.d].particle.randomstartx / 100.0) * t.decalscalemodx;
				t.decalelement[t.d].particle.randomstarty = (t.decalelement[t.d].particle.randomstarty / 100.0) * t.decalscalemody;
				t.decalelement[t.d].particle.randomstartz = (t.decalelement[t.d].particle.randomstartz / 100.0) * t.decalscalemodx;
				t.decalelement[t.d].particle.linearmotionx = (t.decalelement[t.d].particle.linearmotionx / 100.0) * t.decalscalemodx;
				t.decalelement[t.d].particle.linearmotiony = (t.decalelement[t.d].particle.linearmotiony / 100.0) * t.decalscalemody;
				t.decalelement[t.d].particle.linearmotionz = (t.decalelement[t.d].particle.linearmotionz / 100.0) * t.decalscalemodx;
				t.decalelement[t.d].particle.lineargravity = (t.decalelement[t.d].particle.lineargravity / 100.0) * t.decalscalemody;
				t.decalelement[t.d].particle.randommotionx = (t.decalelement[t.d].particle.randommotionx / 100.0) * t.decalscalemodx;
				t.decalelement[t.d].particle.randommotiony = (t.decalelement[t.d].particle.randommotiony / 100.0) * t.decalscalemody;
				t.decalelement[t.d].particle.randommotionz = (t.decalelement[t.d].particle.randommotionz / 100.0) * t.decalscalemodx;
				t.decalelement[t.d].particle.camerazshift = (t.decalelement[t.d].particle.camerazshift / 100.0) * t.decalscalemodx;
			}
			t.decalelement[t.d].fadestarttime = MAXTimer();
			if (t.decalelement[t.d].orient == 8 || t.decalelement[t.d].orient == 13)
			{
				t.decalelement[t.d].fadespan = 750 * (100.0 / t.decalelement[t.d].particle.lifeincrement);
			}
			else
			{
				if (t.decalelement[t.d].orient == 11)
				{
					t.decalelement[t.d].fadespan = 50;
				}
				else
				{
					t.decalelement[t.d].fadespan = 200;
				}
			}
			entity_assignentityparticletodecalelement ();
			if (t.decalelement[t.d].orient == 7 || t.decalelement[t.d].orient == 8 || t.decalelement[t.d].orient == 13)
			{
				//  decal based particle adjust the Y pos, mirror and IX
				t.decalelement[t.d].xpos = t.decalelement[t.d].xpos - ((Rnd(t.decalelement[t.d].particle.randomstartx) - (t.decalelement[t.d].particle.randomstartx / 2)) / 100.0);
				t.decalelement[t.d].ypos = t.decalelement[t.d].ypos - ((Rnd(t.decalelement[t.d].particle.randomstarty) - (t.decalelement[t.d].particle.randomstarty / 2)) / 100.0);
				t.decalelement[t.d].zpos = t.decalelement[t.d].zpos - ((Rnd(t.decalelement[t.d].particle.randomstartz) - (t.decalelement[t.d].particle.randomstartz / 2)) / 100.0);
				t.decalelement[t.d].particleix = ((t.decalelement[t.d].particle.randommotionx / 2) - Rnd(t.decalelement[t.d].particle.randommotionx)) / 100.0;
				t.decalelement[t.d].particleiy = ((t.decalelement[t.d].particle.randommotiony / 2) - Rnd(t.decalelement[t.d].particle.randommotiony)) / 100.0;
				t.decalelement[t.d].particleiz = ((t.decalelement[t.d].particle.randommotionz / 2) - Rnd(t.decalelement[t.d].particle.randommotionz)) / 100.0;
				if (t.decal[t.decalid].particle.mirrormode == 0)  t.decalelement[t.d].particlemirror = 0;
				if (t.decal[t.decalid].particle.mirrormode == 1)  t.decalelement[t.d].particlemirror = 1;
				if (t.decal[t.decalid].particle.mirrormode == 2)  t.decalelement[t.d].particlemirror = Rnd(1);
			}
			//  prepare decal object
			t.tobj = t.decalelement[t.d].obj;
			if (t.decalscalemodx == 0)
			{
				t.decalelement[t.d].particle.offsety = t.decal[t.decalid].particle.offsety;
			}
			else
			{
				t.decalelement[t.d].particle.offsety = (t.decal[t.decalid].particle.offsety / 100.0) * (t.decalscalemody + 0.0);
			}
			PositionObject (t.tobj, t.decalelement[t.d].xpos, t.decalelement[t.d].ypos + t.decalelement[t.d].particle.offsety, t.decalelement[t.d].zpos);
			//  can use root limb to rotate around Z (muzzle flash, etc)
			RotateLimb (t.tobj, 0, 0, 0, 0);
			//  face camera or leave as is
			if (t.decalelement[t.d].orient == 0)
			{
				PointObject (t.tobj, CameraPositionX(), CameraPositionY(), CameraPositionZ());
				XRotateObject (t.tobj, 0); ZRotateObject (t.tobj, 0);
			}
			if (t.decalelement[t.d].orient == 1)
			{
				RotateObject (t.tobj, t.decalorientx_f, t.decalorienty_f, t.decalorientz_f);
			}
			if (t.decalelement[t.d].orient == 2)
			{
				RotateObject (t.tobj, 90, 0, 0);
			}
			if (t.decalelement[t.d].orient == 3)
			{
				PointObject (t.tobj, CameraPositionX(), CameraPositionY(), CameraPositionZ());
			}
			if (t.decalelement[t.d].orient == 4)
			{
				PointObject (t.tobj, CameraPositionX(), CameraPositionY(), CameraPositionZ());
				MoveObject (t.tobj, 5.0);
			}
			if (t.decalelement[t.d].orient == 5)
			{
				PointObject (t.tobj, CameraPositionX(), CameraPositionY(), CameraPositionZ());
				MoveObject (t.tobj, 15.0);
			}
			if (t.decalelement[t.d].orient == 7 || t.decalelement[t.d].orient == 8 || t.decalelement[t.d].orient == 13)
			{
				//  spawn a decal based particle fragment (for new flame and fire) (once)
				PointObject (t.tobj, CameraPositionX(), CameraPositionY(), CameraPositionZ());
				MoveObject (t.tobj, t.decalelement[t.d].particle.camerazshift / 100.0);
				t.decalelement[t.d].xpos = ObjectPositionX(t.tobj);
				t.decalelement[t.d].ypos = ObjectPositionY(t.tobj);
				t.decalelement[t.d].zpos = ObjectPositionZ(t.tobj);
				if (t.decalelement[t.d].orient == 8)
				{
					//  single image rotate Z~360
					RotateLimb (t.tobj, 0, 0, 0, Rnd(360));
				}
			}
			if (t.decalelement[t.d].orient == 11)
			{
				//  231213 - single frame muzzle image with fade
				RotateLimb (t.tobj, 0, 0, 0, Rnd(360));
				PointObject (t.tobj, CameraPositionX(), CameraPositionY(), CameraPositionZ());
				MoveObject (t.tobj, 5.0 + (t.decalforward / 10.0));
			}
			EnableObjectZBias (t.tobj, 0.0, 0.0);
			//  texture for this decal type
			if (t.decalelement[t.d].orient == 13)
			{
				TextureObject (t.tobj, t.huddamage.bloodstart + Rnd(t.huddamage.maxbloodsplats));
			}
			else
			{
				TextureObject (t.tobj, t.decal[t.decalid].imageid);
			}
			//  scale the decal
			if (t.decalscalemodx == 0)
			{
				t.decalelement[t.d].scalemodx = t.decalelement[t.d].particle.scale * (t.decalelement[t.d].particle.scaleonlyx / 100.0);
				t.decalelement[t.d].scalemody = t.decalelement[t.d].particle.scale;
			}
			else
			{
				t.decalelement[t.d].scalemodx = t.decalscalemodx * (t.decalelement[t.d].particle.scale / 100.0) * (t.decalelement[t.d].particle.scaleonlyx / 100.0);
				t.decalelement[t.d].scalemody = t.decalscalemody * (t.decalelement[t.d].particle.scale / 100.0);
			}
			ScaleObject (t.tobj, t.decalelement[t.d].scalemodx, t.decalelement[t.d].scalemody, 100);
			SetAlphaMappingOn (t.tobj, 0);
			t.decalelement[t.d].alphavalue = 0;
			t.decalelement[t.d].uvframe = -1;
			//SetBlend
			//  UV data prep
			if (t.decal[t.decalid].across > 1)
			{
				t.qx_f = 1.0 / (t.decal[t.decalid].across + 0.0);
				t.qy_f = 1.0 / (t.decal[t.decalid].across + 0.0);
			}
			else
			{
				t.qx_f = 1.0;
				t.qy_f = 1.0;
			}
			if (t.decalelement[t.d].orient == 7 && t.decalelement[t.d].particlemirror == 1)
			{
				//  decal based particle can mirror the image
				t.qx_f = t.qx_f * -1;
			}
			SetObjectEffect (t.tobj, g.decaleffectoffset);
			SetObjectCull (t.tobj, 0);
			DisableObjectZWrite (t.tobj);
			SetObjectMask (t.tobj, 1);

			LockVertexDataForLimb (t.tobj, 0);
			SetVertexDataUV (0, t.qx_f, 0);
			SetVertexDataUV (1, 0, 0);
			SetVertexDataUV (2, t.qx_f, t.qy_f);
			SetVertexDataUV (3, 0, 0);
			SetVertexDataUV (4, 0, t.qy_f);
			SetVertexDataUV (5, t.qx_f, t.qy_f);
			UnlockVertexData ();

			//  show decal object
			sObject* pObject = GetObjectData(t.tobj);
			if (pObject)
			{
				uint64_t mat = pObject->ppMeshList[0]->wickedmaterialindex;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(mat);
				if (pObjectMaterial)
				{
					pObjectMaterial->userBlendMode = (BLENDMODE)g_iBlendMode;
					pObjectMaterial->SetDirty();
				}
			}
			ShowObject (t.tobj);
		}
	}
	else
	{
		t.tobj=0;
	}
}

void decalelement_continue ( void )
{
	// decal is looping, merely need to continue the current decal
	t.d = t.currentdecald;
	if (t.decalelement[t.d].newparticle.emitterid <= -1)
	{
		// no loop control required - built-in!
	}
	else
	{
		if (t.d > 0 && t.decalelement[t.d].originator == t.originatore)
		{
			// simply reset decal
			t.decalelement[t.d].frame = 0;
		}
		else
		{
			// different, so recreate
			t.decalforward = t.decalelement[t.d].decalforward;
			decalelement_create ();
		}
	}
}

void decalelement_control ( void )
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	// (GGMAX 2.27: the pool-growth budget is NOT refilled here. It refills off the device frame
	//  counter inside decal_growbudgeted — this tick does not run in every app state, and relying
	//  on it disabled growth entirely in the editor. One source of truth; see §24.2.)

	// True camera position (returns tcamerapositionx#,tcamerapositiony#,tcamerapositionz#)
	entity_gettruecamera ( );

	// Control all decal activity
	for ( t.f = 1 ; t.f<=  g.decalelementmax; t.f++ )
	{
		if ( t.decalelement[t.f].active == 1)
		{
			int decalid = t.decalelement[t.f].decalid;

			// error trap decalIDs that exceed the current decal array
			if (t.decalid >= t.decal.size() || decalid >= t.decal.size())
				continue;

			// decal speed machine independent
			t.decaltimeelapsed_f = g.timeelapsed_f;
			if (t.decalelement[t.f].newparticle.emitterid == -2 || t.decalelement[t.f].newparticle.emitterid >= 0)
			{
				// initial one loads/clones an instance of the particle
				if (t.decalelement[t.f].newparticle.emitterid == -2)
				{
					// will force a load inside 'newparticle_updateparticleemitter'
					t.decalelement[t.f].newparticle.emitterid = -1;
					t.decalelement[t.f].framedelay = 0;
				}

				// update new particle decal
				GGMATRIX pmatBaseRotation;
				float fScale = -80.0f;
				float fX = t.decalelement[t.f].xpos;
				float fY = t.decalelement[t.f].ypos;
				float fZ = t.decalelement[t.f].zpos;
				float fRX = 0.0f;
				float fRY = 0.0f;
				float fRZ = 0.0f;
				GGMatrixRotationY(&pmatBaseRotation, GGToRadian(fRY));
				std::map<int, GGVECTOR3>::iterator itUp = g_DecalElementUp.find ( t.f );
				newparticle_updateparticleemitter(&t.decalelement[t.f].newparticle, fScale, fX, fY, fZ, fRX, fRY, fRZ, NULL, true, decalid, itUp != g_DecalElementUp.end() ? &itUp->second : NULL);

				if (t.decal[decalid].newparticle.bWPE)
				{
					t.decalelement[t.f].framedelay = t.decalelement[t.f].framedelay + (t.decaltimeelapsed_f);

					// ★★★ GGMAX 3.37 WPE EMIT WINDOW (2026-09-14, Lee: one shot into water = a splash
					// that repeats and runs 6-8 s).
					//
					// This threshold is a DURATION, and its unit is not what it looks like. Time here is
					// g.timeelapsed_f = ElapsedTime_f * 20 (M-Game_part2.cpp:1541), i.e. ONE REAL SECOND
					// IS 20 UNITS - so the old `>= 100` was 5.0 SECONDS of continuous emission, not the
					// 100 ms it reads as. Frame-rate independent, so it was a flat 5 s on every machine.
					//
					// ★ Why only the WPE path was wrong: the legacy branch below (the `!bWPE` copy of this
					// same test) accumulates `decaltimeelapsed_f * 2 * playspeed_f`, which happens to scale
					// the same 100 down to ~0.8 s for the impact decal. This branch accumulates the RAW
					// value, so it inherited a threshold that had only ever been tuned through that
					// multiplier. Measured, not guessed: 100 / 20 = 5.0 s exactly.
					//
					// Action 7 stops NEW particles and leaves the live ones to finish, so a short window
					// still DISSIPATES rather than snapping off - the 2.28 behaviour below is preserved,
					// it just stops five seconds earlier. Recycling the element at the same moment is
					// safe and returns the pool slot sooner: the emitter entity is cache-owned and is
					// deliberately NOT deleted, so particles already in flight are unaffected.
					//
					// ⚠ Separate CONTENT issue, not fixed here: splash_large/wpe.pe contains FIVE emitters
					// - 'pe-emitter-d', 'pe-emitter-d - Copy', 'pe-emitter-ripple 1' and TWO more Copies of
					// that ripple - against one emitter in impact/wpe.pe. They all burst together, so one
					// splash pays for five. Worth re-authoring, but it is not what made it last 6-8 s.
					const float GG_WPE_EMIT_SECONDS = 0.20f;          // burst window for a decal effect
					const float GG_WPE_EMIT_UNITS   = GG_WPE_EMIT_SECONDS * 20.0f;   // 1 s = 20 units
					if (t.decalelement[t.f].framedelay >= GG_WPE_EMIT_UNITS)
					{
						// GGMAX 2.28 (2026-08-12): STOP EMITTING before abandoning the emitter.
						//
						// The original comment here read "No need to stop effect it will end by
						// itself", and that assumption holds only for a finite burst. A CONTINUOUS
						// emitter — the impact smoke flume — never ends, so the effect looped
						// forever; and because the WPE emitters come from a 5-slot round-robin
						// cache (MAXREADYDECALS, M-Entity_part0.cpp:31) the user could stack
						// exactly five before the oldest was recycled. Reported from live play.
						//
						// Action 7 = SetEmitPaused(true): stops NEW particles but lets the ones
						// already alive finish their lifetime, so the flume DISSIPATES rather than
						// being cut off mid-air. Action 2 (pause) or 6 (hide) would both snap it
						// out of existence.
						// ⚠ Do NOT delete the emitter — it belongs to the ready_decals cache and
						// must survive for reuse; deleting strands a cache slot on a dead entity.
						// ★ Precedent: wickedcalls_part4.cpp:41 records that actions 6/7 were
						// restored in 2.00 precisely because "weapon trails never stopped
						// emitting". The decal path was never updated to use them.
						// See NIGHT_INVESTIGATIONS_2026-08-12.md section A.
						if (t.decalelement[t.f].newparticle.emitterid > 0)
							WickedCall_PerformEmitterAction(7, (uint32_t)t.decalelement[t.f].newparticle.emitterid);
						t.decalelement[t.f].active = 0;
						t.decalelement[t.f].framedelay = 0;
						t.decalelement[t.f].newparticle.emitterid = -1;
					}
				}
				if (!t.decal[decalid].newparticle.bWPE)
				{
					// detect when burst finished so can set active back to zero and remove emitter
					t.decalelement[t.f].framedelay = t.decalelement[t.f].framedelay + (t.decaltimeelapsed_f * 2 * t.decal[t.decalid].playspeed_f);
					if (t.decalelement[t.f].framedelay >= 100)
					{
						// delete used particle
						extern void newparticle_deleteparticleemitter(int);
						newparticle_deleteparticleemitter(t.decalelement[t.f].newparticle.emitterid);
						t.decalelement[t.f].newparticle.emitterid = -1;

						// end instance
						t.decalelement[t.f].active = 0;
					}
				}
			}
			else
			{
				// update legacy decal object
				t.tobj = t.decalelement[t.f].obj; t.tdetonate = 0;
				//  if decal not in visible screen, destroy for performance - off no code
				if (t.tdetonate == 0)
				{
					//  possible movement
					if (t.decalelement[t.f].orient == 7 || t.decalelement[t.f].orient == 8 || t.decalelement[t.f].orient == 13)
					{
						//  decal based particle moves in 3D space
						t.decalelement[t.f].xpos = t.decalelement[t.f].xpos + (((t.decalelement[t.f].particleix / 100.0) * 100.0) * t.decaltimeelapsed_f);
						t.decalelement[t.f].ypos = t.decalelement[t.f].ypos + (((t.decalelement[t.f].particleiy / 100.0) * 100.0) * t.decaltimeelapsed_f);
						t.decalelement[t.f].zpos = t.decalelement[t.f].zpos + (((t.decalelement[t.f].particleiz / 100.0) * 100.0) * t.decaltimeelapsed_f);
						t.decalelement[t.f].xpos = t.decalelement[t.f].xpos + ((t.decalelement[t.f].particle.linearmotionx / 100.0) * t.decaltimeelapsed_f);
						t.decalelement[t.f].ypos = t.decalelement[t.f].ypos + ((t.decalelement[t.f].particle.linearmotiony / 100.0) * t.decaltimeelapsed_f);
						t.decalelement[t.f].zpos = t.decalelement[t.f].zpos + ((t.decalelement[t.f].particle.linearmotionz / 100.0) * t.decaltimeelapsed_f);
						if (t.decalelement[t.f].particle.lineargravity != 0)
						{
							t.decalelement[t.f].particle.linearmotiony = t.decalelement[t.f].particle.linearmotiony - ((t.decalelement[t.f].particle.lineargravity / 100.0) * t.decaltimeelapsed_f * 10.0);
						}
					}
					if (t.decalelement[t.f].orient == 13)
					{
						//  in addition, for blood, move towards player for spray effect (see below)
					}
					//  animation
					t.decalid = t.decalelement[t.f].decalid;
					//  next frame
					t.decalelement[t.f].framedelay = t.decalelement[t.f].framedelay + (t.decaltimeelapsed_f * 2 * t.decal[t.decalid].playspeed_f);
					if (t.decalelement[t.f].framedelay >= 1)
					{
						t.tmultiple = t.decalelement[t.f].framedelay;
						t.decalelement[t.f].framedelay = 0;
						t.decalelement[t.f].frame = t.decalelement[t.f].frame + ((t.decalelement[t.f].particle.lifeincrement / 100.0) * t.tmultiple);
					}
					if (t.decalelement[t.f].orient == 8 || t.decalelement[t.f].orient == 11 || t.decalelement[t.f].orient == 13)
					{
						//  Sin ( gle image fader Timer (  based life ) )
						t.decalelement[t.f].frame = 0;
						if ((int)((MAXTimer() - t.decalelement[t.f].fadestarttime)) > t.decalelement[t.f].fadespan)
						{
							t.tdetonate = 1;
						}
					}
					else
					{
						//  regular frame end detection
						if (t.decalelement[t.f].frame > t.decalelement[t.f].framefinish - 1)
						{
							//  no more animation
							t.decalelement[t.f].frame = t.decalelement[t.f].framefinish - 0.1;
							t.tdetonate = 1;
						}
					}
					//  write UV for correct anim frame
					if (GetVisible(t.tobj) == 1 && GetInScreen(t.tobj) == 1 && t.tdetonate == 0 || t.decalelement[t.f].decalid == t.decalglobal.splashdecalrippleid && t.tdetonate == 0 && ObjectExist(t.tobj))
					{
						float fAlpha = -1.0f;
						if (t.decalelement[t.f].decalid == t.decalglobal.splashdecalrippleid && ObjectExist(t.tobj))  fAlpha = 8;
						//  rotate to face camera if flagged
						if (t.decalelement[t.f].orient == 0)
						{
							PointObject (t.tobj, t.tcamerapositionx_f, t.tcamerapositiony_f, t.tcamerapositionz_f);
							XRotateObject (t.tobj, 0); ZRotateObject (t.tobj, 0);
						}
						if (t.decalelement[t.f].orient == 3 || t.decalelement[t.f].orient == 11 || t.decalelement[t.f].orient == 12)
						{
							PointObject (t.tobj, t.tcamerapositionx_f, t.tcamerapositiony_f, t.tcamerapositionz_f);
						}
						if (t.decalelement[t.f].orient == 7 || t.decalelement[t.f].orient == 8 || t.decalelement[t.f].orient == 13)
						{
							//  decal based particle faces camera always and moves
							t.ty = t.decalelement[t.f].particle.offsety;
							PositionObject (t.tobj, t.decalelement[t.f].xpos, t.decalelement[t.f].ypos + t.ty, t.decalelement[t.f].zpos);
							PointObject (t.tobj, t.tcamerapositionx_f, t.tcamerapositiony_f, t.tcamerapositionz_f);
							if (t.decalelement[t.f].orient == 13)
							{
								//  spray towards player too
								MoveObject (t.tobj, 2.0 * t.decaltimeelapsed_f);
								t.decalelement[t.f].xpos = ObjectPositionX(t.tobj);
								t.decalelement[t.f].ypos = ObjectPositionY(t.tobj);
								t.decalelement[t.f].zpos = ObjectPositionZ(t.tobj);
							}
						}
						if (t.decalelement[t.f].orient == 0 || t.decalelement[t.f].orient == 1 || t.decalelement[t.f].orient == 3 || t.decalelement[t.f].orient == 4 || t.decalelement[t.f].orient == 5)
						{
							fAlpha = 100.0;
						}
						if (t.decalelement[t.f].orient == 2)
						{
							fAlpha = 100.0;
							// fade in and out over life of decal animation
							t.pt_f = t.decalelement[t.f].framefinish;
							t.p_f = (t.decalelement[t.f].framefinish - t.decalelement[t.f].frame) / t.pt_f;
							if (t.p_f >= 0.5)
							{
								fAlpha = 100.0 - ((t.p_f - 0.5) * 200.0);
							}
							else
							{
								fAlpha = t.p_f * 200.0;
							}
						}
						if (t.decalelement[t.f].orient == 7)
						{
							//  fade over life of decal animation
							t.pt_f = t.decalelement[t.f].framefinish;
							t.p_f = (t.decalelement[t.f].framefinish - t.decalelement[t.f].frame) / t.pt_f;
							fAlpha = Sin(t.p_f * 180) * t.decalelement[t.f].particle.alphaintensity;
						}
						if (t.decalelement[t.f].orient == 8 || t.decalelement[t.f].orient == 11 || t.decalelement[t.f].orient == 13)
						{
							//  single image fader
							t.tfadeperc_f = ((MAXTimer() - t.decalelement[t.f].fadestarttime) + 0.0) / (t.decalelement[t.f].fadespan + 0.0);
							if (t.tfadeperc_f < 0.0)  t.tfadeperc_f = 0.0;
							if (t.tfadeperc_f > 1.0)  t.tfadeperc_f = 1.0;
							t.tfinalalphavalue_f = t.decalelement[t.f].particle.alphaintensity / 100.0;
							if (t.decalelement[t.f].burstloop > 0)  t.tfinalalphavalue_f = t.tfinalalphavalue_f * 0.7;
							fAlpha = (100.0 - (t.tfadeperc_f * 100.0)) * t.tfinalalphavalue_f;
						}
						if (t.decalelement[t.f].orient == 12)
						{
							//  multi-image fader (fades towards end)
							t.pt_f = t.decalelement[t.f].framefinish;
							t.p_f = (t.decalelement[t.f].framefinish - t.decalelement[t.f].frame) / t.pt_f;
							fAlpha = Sin(t.p_f * 180) * 100;
						}
						// only update alpha when it changes
						if (fAlpha >= 0.0f && fAlpha != t.decalelement[t.f].alphavalue)
						{
							WickedCallProbeScope probe(WICKEDCALL_PROBE_DECAL_FADE);
							SetAlphaMappingOn (t.tobj, fAlpha);
							t.decalelement[t.f].alphavalue = fAlpha;
						}
						//  decal animation setting
						if (t.decalelement[t.f].orient == 7 && t.decalelement[t.f].particle.animated != 1 && t.decalelement[t.f].decalid != t.decalglobal.splashdecalrippleid)
						{
							//  decal based particle only uses first animation frame
							t.tframe = 0;
						}
						else
						{
							t.tframe = t.decalelement[t.f].frame;
						}
						//  animation UVs
						if (t.decal[t.decalid].across > 1)
						{
							t.ty = t.tframe / t.decal[t.decalid].across; t.tx = t.tframe - (t.ty * t.decal[t.decalid].across);
							t.qx_f = 1.0 / (t.decal[t.decalid].across + 0.0); t.tx_f = t.tx * t.qx_f;
							t.qy_f = 1.0 / (t.decal[t.decalid].across + 0.0); t.ty_f = t.ty * t.qy_f;
						}
						else
						{
							t.qx_f = 1.0; t.tx_f = 0;
							t.qy_f = 1.0; t.ty_f = 0;
						}
						if (t.decalelement[t.f].orient == 7 && t.decalelement[t.f].particlemirror == 1)
						{
							//  decal based particle can mirror the image
							t.tx_f = (t.tx * t.qx_f) + t.qx_f;
					}
						// only update UV frame when it changes
						if (t.tframe != t.decalelement[t.f].uvframe)
						{
							decal_setelementuvframe (t.tobj, t.tframe, t.decal[t.decalid].across, t.decal[t.decalid].down);
							t.decalelement[t.f].uvframe = t.tframe;
						}
				}
			}
				//  detonate trigger
				if (t.tdetonate == 1)
				{
					HideObject (t.decalelement[t.f].obj);
					t.decalelement[t.f].originator = 0;
					t.decalelement[t.f].active = 0;
					if (t.decalelement[t.f].burstloop > 0)
					{
						//  re-create decal if burstloop active
						t.decalelement[t.f].burstloop = t.decalelement[t.f].burstloop - 1;
						if (t.decalelement[t.f].burstloop > 0)
						{
							t.decalid = t.decalelement[t.f].decalid;
							t.toriginatorobj = t.decalelement[t.f].originatorobj;
							if (t.toriginatorobj > 0)
							{
								//  check the object exists
								if (ObjectExist(t.toriginatorobj) == 1)
								{
									g.decalx = ObjectPositionX(t.toriginatorobj);
									g.decaly = ObjectPositionY(t.toriginatorobj);
									g.decalz = ObjectPositionZ(t.toriginatorobj);
								}
								else
								{
									g.decalx = t.decalelement[t.f].xpos;
									g.decaly = t.decalelement[t.f].ypos;
									g.decalz = t.decalelement[t.f].zpos;
								}
							}
							else
							{
								g.decalx = t.decalelement[t.f].xpos;
								g.decaly = t.decalelement[t.f].ypos;
								g.decalz = t.decalelement[t.f].zpos;
							}
							t.decalscalemodx = 0;
							t.decalorient = t.decalelement[t.f].orient;
							t.originatore = -1;
							t.decalburstloop = t.decalelement[t.f].burstloop;
							t.decalforward = t.decalelement[t.f].decalforward;
							decalelement_create ();
							t.decalburstloop = 0;
						}
					}
				}
			}
		}
	}
}

void decal_triggermaterialdebris ( void )
{
	//  takes tsoundmaterial,tsx#,tsy#,tsz#
	if (  t.tsoundmaterial>0 ) 
	{
		t.decalid=t.material[t.tsoundmaterial-1].decalid;
		if (  t.tsy_f<g.waterheight_f+10  )  t.decalid = 0;
		if (  t.decalid>0 ) 
		{
			//  create the decal for this event
			t.decalscalemodx=0;
			t.decalorient=4 ; g.decalx=t.tsx_f ; g.decaly=t.tsy_f ; g.decalz=t.tsz_f;
			t.decalforward=0;
			t.originatore=-1 ; decalelement_create ( );
		}
		t.tsoundmaterial=0;
	}
}

void decal_find (void)
{
	// Load all decals
	for (t.decalid = 1; t.decalid <= g.decalmax; t.decalid++)
	{
		if (t.decal[t.decalid].name_s == t.decal_s)  break;
	}
	if (t.decalid > g.decalmax)
	{
		// if not an exact match, look for a pattern match to the decal given and see if we have a secondary match (some older gunspecs assume the root)
		if (strlen(t.decal_s.Get()) > 0)
		{
			LPSTR pPattern = t.decal_s.Get();
			for (t.decalid = 1; t.decalid <= g.decalmax; t.decalid++)
			{
				LPSTR pLookAt = t.decal[t.decalid].name_s.Get();
				if (strnicmp (pLookAt + strlen(pLookAt) - strlen(pPattern), pPattern, strlen(pPattern)) == NULL) break;
			}
		}
		else
		{
			t.decalid = -1;
		}
	}
	if (t.decalid > g.decalmax)
	{
		t.decalid = -1;
	}
}

void decal_activatedecalsfromentities ( void )
{
	//  Only flag those present in level
	for ( t.tdecalid = 1 ; t.tdecalid<=  g.decalmax; t.tdecalid++ )
	{
		t.tokay=0;
		if (  t.tdecalid == t.decalglobal.splashdecalrippleid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.splashdecalsmallid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.splashdecallargeid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.splashdecalmistyid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.splashdecaldropletsid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.splashdecalfoamid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.dustflumeid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.impactid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.bloodsplatid  )  t.tokay = 1;
		if (  t.tdecalid == t.decalglobal.newexplosion )  t.tokay = 1;
		if (  t.tokay == 1 )
		{
			t.decal[t.tdecalid].active=1;
		}
		else
		{
			t.decal[t.tdecalid].active=0;
		}
	}
	for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
	{
		t.entid=t.entityelement[t.e].bankindex;

		if (t.entityelement[t.e].eleprof.explodable_decalname.Len() > 0)
		{
			cstr explodename = t.entityelement[t.e].eleprof.explodable_decalname;
			cstr lexplodename = explodename.Lower();
			if (lexplodename.Len() > 0)
			{
				for (int i = 1; i <= g.decalmax; i++)
				{
					cstr ldecal = t.decal[i].name_s.Lower();
					if (ldecal == lexplodename)
					{
						t.decal[i].active = 1;
						t.decal[i].newparticle.iMaxCache = 2; //PE: For now only 2 cached custom explosions.
						break;
					}
				}
			}
		}

		if (  t.entid>0 ) 
		{
			//  per-entity decal list
			if (  t.entityprofile[t.entid].decalmax>0 ) 
			{
				for ( t.tq = 0 ; t.tq<=  t.entityprofile[t.entid].decalmax-1; t.tq++ )
				{
					t.tdecalid=t.entitydecal[t.entid][t.tq];
					//PE: Bug was never activated. t.entityprofile[t.entid].decalmax)
					if (t.tdecalid <= g.decalmax)
					{
						//PE: Never cache activate default.
						if (t.decal[t.tdecalid].newparticle.emittername != "particlesbank/default")
						{
							//particlesbank/default
							t.decal[t.tdecalid].active = 1;
							if (t.entityprofile[t.entid].bloodscorch == 0)
								t.decal[t.tdecalid].newparticle.iMaxCache = 2; //PE: Only cache 2 custom decals.
						}
						else
						{
							//PE: Unless this is a impact from the fpe.
							if ( t.entitydecal_s[t.entid][t.tq].Len() > 0)
							{
								t.decal[t.tdecalid].active = 1;
								if (t.entityprofile[t.entid].bloodscorch == 0)
									t.decal[t.tdecalid].newparticle.iMaxCache = 2; //PE: Only cache 2 custom decals.
							}
						}
					}
				}
			}
			//  decals through associated weapon
			t.tfoundgunid=t.entityprofile[t.entid].hasweapon;
			if (  t.tfoundgunid>0 ) 
			{
				for ( t.talt = 0 ; t.talt<=  1; t.talt++ )
				{
					t.tdecalid=g.firemodes[t.tfoundgunid][t.talt].decalid;
					t.decal[t.tdecalid].active=1;
				}
			}
		}
	}
	// also activate decals from guns
	for (int gunid = 1; gunid <= g.gunmax; gunid++)
	{
		if (t.gun[gunid].activeingame == 1)
		{
			for (t.talt = 0; t.talt <= 1; t.talt++)
			{
				t.tdecalid = g.firemodes[gunid][t.talt].decalid;
				t.decal[t.tdecalid].active = 1;
			}
		}
	}
}

void decal_triggerwatersplash ( void )
{
	//  location of water splash passed in decalx y z
	//  SK Uses tInScale# to modify size.
	t.originatore=-1;
	t.decalforward=0;
	t.decalscalemodx=(40+Rnd(20))*t.tInScale_f ; t.decalscalemody=(40+Rnd(20))*t.tInScale_f;
	if (t.decal[t.decalglobal.splashdecallargeid].newparticle.bWPE)
	{
		t.decalid = t.decalglobal.splashdecallargeid; t.decalorient = 0; decalelement_create();
	}
	else
	{
		t.decalid = t.decalglobal.splashdecalrippleid; t.decalorient = 2; decalelement_create();
		if (Rnd(1) == 0)
		{
			t.decalid = t.decalglobal.splashdecalsmallid; t.decalorient = 0; decalelement_create();
		}
		else
		{
			t.decalid = t.decalglobal.splashdecallargeid; t.decalorient = 0; decalelement_create();
		}
		t.decalid = t.decalglobal.splashdecalmistyid; t.decalorient = 12; decalelement_create();
		t.decalid = t.decalglobal.splashdecaldropletsid; t.decalorient = 8; decalelement_create();
		t.decalid = t.decalglobal.splashdecaldropletsid; t.decalorient = 8; decalelement_create();
		t.decalid = t.decalglobal.splashdecaldropletsid; t.decalorient = 8; decalelement_create();
		t.decalid = t.decalglobal.splashdecaldropletsid; t.decalorient = 8; decalelement_create();
		t.decalid = t.decalglobal.splashdecaldropletsid; t.decalorient = 8; decalelement_create();
		t.decalid = t.decalglobal.splashdecalfoamid; t.decalorient = 2; decalelement_create();
	}
}

void decal_triggerwaterripple ( void )
{
	//  location of water ripple passed in decalx y z
	t.originatore=-1 ; t.decalscalemodx=200 ; t.decalscalemody=200;
	t.decalforward=0;
	t.decalid = t.decalglobal.splashdecalrippleid; t.decalorient = 2; decalelement_create ( );
}

void decal_triggerwaterripplesize(void)
{
	//  location of water ripple passed in decalx y z
	t.originatore = -1;/* t.decalscalemodx = 200; t.decalscalemody = 200;*/
	t.decalforward = 0;
	t.decalid = t.decalglobal.splashdecalrippleid; t.decalorient = 2; decalelement_create();
}

void decal_triggerimpact ( void )
{
	//  location of dust effect passed in decalx y z
	t.originatore=-1;
	t.decalscalemodx=20+Rnd(10) ; t.decalscalemody=20;
	t.decalforward=0;
	t.decalid=t.decalglobal.impactid ; t.decalorient=5  ; decalelement_create ( );
}

void decal_triggerbloodsplat ( void )
{
	//  location of effect passed in decalx y z
	t.originatore=-1;
	t.decalscalemodx=40+Rnd(20) ; t.decalscalemody=40+Rnd(20);
	t.decalforward=0;
	#ifdef DETECTANDUSENEWPARTICLEDECALS
	t.decalid = t.decalglobal.bloodsplatid; t.decalorient = 0; decalelement_create ();
	#else
	t.decalid = t.decalglobal.bloodsplatid; t.decalorient = 13; decalelement_create ();
	#endif
}

void decal_triggermaterialdecal ( void )
{
	//  location of material decal effect passed in decalx y z
	t.originatore=-1;
	t.decalscalemodx=20+Rnd(10) ; t.decalscalemody=20;
	t.decalforward=0;
	t.decalid=t.material[t.tttriggerdecalimpact-10].decalid;
	t.decalorient=5 ; decalelement_create ( );
}

int loaddecal ( char* tfile_s, int decalid )
{
	int timg = 0;
	timg=loadinternalimagecompress(tfile_s,5);
	t.decal[decalid].imageid=timg;
	return timg;
}
