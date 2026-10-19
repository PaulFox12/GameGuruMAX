//----------------------------------------------------
//--- GAMEGURU - M-Lighting
//----------------------------------------------------

#include "stdafx.h"
#include "gameguru.h"

int spot_lights_count = 0;
int point_lights_count = 0;

void lighting_refresh ( void )
{
	// by default, no override
	t.lighting.override=0;

	spot_lights_count = 0;
	point_lights_count = 0;
	// first ensure old wicked resources for lights removed
	for (int l = 1; l <= g.infinilightmax; l++)
	{
		infinilighttype* pLightPtr = &t.infinilight[l];
		if (pLightPtr->wickedlightindex > 0)
		{
			WickedCall_DeleteLight(pLightPtr->wickedlightindex);
			pLightPtr->wickedlightindex = 0;
		}
	}

	// create infinilight list from all active entities
	g.infinilightmax=0;
	for ( t.tle = 1 ; t.tle <= g.entityelementlist; t.tle++ )
	{
		t.tlentid=t.entityelement[t.tle].bankindex;
		if ( t.tlentid>0 ) 
		{
			if ( t.entityprofile[t.tlentid].ismarker == 2 ) 
			{
				++g.infinilightmax;
				Dim ( t.infinilight, g.infinilightmax  );
				t.infinilight[g.infinilightmax].used=1;
				t.infinilight[g.infinilightmax].type=t.entityelement[t.tle].staticflag;

				//PE: Add dynamic spot light support.
				t.infinilight[g.infinilightmax].is_spot_light = t.entityelement[t.tle].eleprof.usespotlighting;

				//PE: Get the spot light direction vector.
				sObject* pObject = g_ObjectList[t.entityelement[t.tle].obj];
				if (pObject) 
				{
					//PE: In wicked we use the rotation vector for spot light.
					t.infinilight[g.infinilightmax].f_angle_x = pObject->position.vecRotate.x;
					t.infinilight[g.infinilightmax].f_angle_y = pObject->position.vecRotate.y;
					t.infinilight[g.infinilightmax].f_angle_z = pObject->position.vecRotate.z;
				}
				t.infinilight[g.infinilightmax].x=t.entityelement[t.tle].x;
				t.infinilight[g.infinilightmax].y=t.entityelement[t.tle].y;
				t.infinilight[g.infinilightmax].z=t.entityelement[t.tle].z;
				t.infinilight[g.infinilightmax].range = t.entityelement[t.tle].eleprof.light.range;
				t.infinilight[g.infinilightmax].spotlightradius = t.entityelement[t.tle].eleprof.light.offsetup;
				t.infinilight[g.infinilightmax].id=0;
				t.infinilight[g.infinilightmax].dist=0;
				t.infinilight[g.infinilightmax].colrgb.r=RgbR(t.entityelement[t.tle].eleprof.light.color);
				t.infinilight[g.infinilightmax].colrgb.g=RgbG(t.entityelement[t.tle].eleprof.light.color);
				t.infinilight[g.infinilightmax].colrgb.b=RgbB(t.entityelement[t.tle].eleprof.light.color);
				if (t.entityelement[t.tle].eleprof.castshadow == 1 )
					t.infinilight[g.infinilightmax].bCanShadow = false;
				else
					t.infinilight[g.infinilightmax].bCanShadow = true;
				
				t.entityelement[t.tle].eleprof.light.islit=1;
				t.infinilight[g.infinilightmax].islit=t.entityelement[t.tle].eleprof.light.islit;
				t.infinilight[g.infinilightmax].e=t.tle;
				t.entityelement[t.tle].eleprof.light.index=g.infinilightmax;

				t.infinilight[g.infinilightmax].fLightHasProbe = t.entityelement[t.tle].eleprof.light.fLightHasProbe;
				int iLightType = 1;
				if (t.infinilight[g.infinilightmax].is_spot_light == true)
				{
					iLightType = 2;
					spot_lights_count++;
				}
				else
				{
					point_lights_count++;
				}
				t.infinilight[g.infinilightmax].wickedlightindex = WickedCall_AddLight(iLightType);
			}
		}
		else
		{
			t.entityelement[t.tle].eleprof.light.islit=0;
			t.entityelement[t.tle].eleprof.light.index=0;
		}
	}

	// GG: the lights entities carry (entitylight in the .fpe), after the light markers' so their numbers don't change; each
	// takes its range and colour from the .fpe, and lighting_loop places it with its entity every frame
	for ( t.tle = 1 ; t.tle <= g.entityelementlist; t.tle++ )
	{
		t.tlentid=t.entityelement[t.tle].bankindex;
		if ( t.tlentid <= 0 || t.entityprofile[t.tlentid].ismarker != 0 || !t.entityprofile[t.tlentid].entitylight ) continue;
		++g.infinilightmax;
		Dim ( t.infinilight, g.infinilightmax  );
		infinilighttype* pLightPtr = &t.infinilight[g.infinilightmax];
		pLightPtr->used = 1;
		pLightPtr->type = t.entityelement[t.tle].staticflag;
		pLightPtr->is_spot_light = false;
		pLightPtr->bEntityLight = true;
		pLightPtr->f_angle_x = 0;
		pLightPtr->f_angle_y = 0;
		pLightPtr->f_angle_z = 0;
		pLightPtr->x = t.entityelement[t.tle].x;
		pLightPtr->y = t.entityelement[t.tle].y;
		pLightPtr->z = t.entityelement[t.tle].z;
		pLightPtr->range = t.entityprofile[t.tlentid].light.range;
		pLightPtr->spotlightradius = 45.0f;
		pLightPtr->id = 0;
		pLightPtr->dist = 0;
		pLightPtr->colrgb.r = RgbR(t.entityprofile[t.tlentid].light.color);
		pLightPtr->colrgb.g = RgbG(t.entityprofile[t.tlentid].light.color);
		pLightPtr->colrgb.b = RgbB(t.entityprofile[t.tlentid].light.color);
		pLightPtr->bCanShadow = t.entityprofile[t.tlentid].entitylightshadows != 0;
		pLightPtr->islit = 1;
		pLightPtr->e = t.tle;
		pLightPtr->fLightHasProbe = 0;
		t.entityelement[t.tle].eleprof.light.index = g.infinilightmax;
		point_lights_count++;
		pLightPtr->wickedlightindex = WickedCall_AddLight(1);
	}

	// clear old infini light history
	t.nearestlightindex[0]=0;
	t.nearestlightindex[1]=0;
	t.nearestlightindex[2]=0;
	t.nearestlightindex[3]=0;
	t.nearestlightindex[4]=0;
}

// GG: entities added or deleted in a run (the spline tool's layers placing hundreds of lamps) rebuild the light list once,
// when the hold ends, not once each
int g_iLightingRefreshHold = 0;
bool g_bLightingRefreshHeld = false;

void lighting_refreshorhold ( void )
{
	if ( g_iLightingRefreshHold > 0 ) g_bLightingRefreshHeld = true;
	else lighting_refresh ( );
}

void lighting_holdrefresh ( bool bHold )
{
	if ( bHold ) { g_iLightingRefreshHold++; return; }
	if ( g_iLightingRefreshHold > 0 ) g_iLightingRefreshHold--;
	if ( g_iLightingRefreshHold == 0 && g_bLightingRefreshHeld )
	{
		g_bLightingRefreshHeld = false;
		lighting_refresh ( );
	}
}
