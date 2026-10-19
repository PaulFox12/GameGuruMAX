int iInstancedTotal = 0;

// puts an entity's authored materials on its object, mesh by mesh, as its .fpe gives them, or its element's own WEMaterial
// when it has one: textures, base and emissive colour, emissive strength, roughness, metalness, reflectance, normal
// strength, alpha and transparency. Instanced meshes are left alone, as they use their model's master object's
// materials. entity_prepareobj does it for each new entity object, and entity_resetmaterial when a script asks
void entity_applymaterial ( int tentid, int tte, int tobj )
{
	if (t.entityelement[tte].eleprof.WEMaterial.MaterialActive)
	{
		WickedSetEntityId(tentid);
		WickedSetElementId(tte);
	}
	if (!t.entityprofile[tentid].bIsDecal)
		SetAlphaMappingOn(tobj, 100);
	WickedSetEntityId(-1);
	WickedSetElementId(0);

	// set transparency mode (after 'set alpha mapping on' as it messes with transparency flag)
	if ( t.entityprofile[tentid].ismarker == 0 )
	{
		// PE: Wicked material can overwrite objects settings.
		// LB: always prepare object with TextureMesh!
		// LB: need to restore ALL WEMaterial settings here when preparing the object
		WickedSetEntityId(tentid);
		WickedSetElementId(tte);
		// LB: apply WEMaterial to all meshes of this object, not just the first one
		// LB: Setting object transparency defaults here (so not everything is transparent), but the TextureMesh can then set per-mesh transparency :)
		SetObjectTransparency(tobj, t.entityelement[tte].eleprof.WEMaterial.bTransparency[0]);
		sObject* pObject = g_ObjectList[tobj];
		for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
		{
			sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
			if (pMesh)
			{
				// set properties of mesh
				WickedSetMeshNumber(iMeshIndex);

				// sets ALL properties of each mesh from WEMaterial
				if (pMesh->bInstanced && pMesh->wickedmaterialindex == 0 && pMesh->master_wickedmaterialindex > 0 )
				{
					//PE: No need to texture Instanced objects.
					iInstancedTotal++;
				}
				else
				{
					// from WE materials
					WickedCall_TextureMesh(pMesh);
					SpawnProbe_Sub("texture mesh");

					// and must restore mesh transparency flag
					bool bTransparent = WickedGetTransparent();
					pMesh->bTransparency = bTransparent;
				}
			}
		}
		WickedSetEntityId(-1);
		WickedSetElementId(0);
	}
	else
	{
		sObject* pObject = g_ObjectList[tobj];
		if (pObject)
		{
			WickedCall_TextureObject(pObject, NULL);
			SpawnProbe_Sub("texture object");
		}
	}
}

// ResetEntityMaterial: an entity's materials as the level load gave them, after scripts have changed them. Materials are
// shared by every entity of a model, as the material setters' changes are, so it resets them for all of those; the
// per-instance values (SetEntityInstanceEmissive, the tint, limb alpha) are left alone. An instanced entity's meshes use
// its model's master object's materials, which are set again as the entity bank sets them up
void entity_resetmaterial ( int e )
{
	if (e <= 0 || e >= (int)t.entityelement.size()) return;
	int entid = t.entityelement[e].bankindex;
	int obj = t.entityelement[e].obj;
	if (entid <= 0 || obj <= 0 || ObjectExist(obj) == 0) return;
	sObject* pObject = GetObjectData(obj);
	if (!pObject) return;

	bool bInstanced = false;
	for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
		if (pMesh && pMesh->bInstanced && pMesh->wickedmaterialindex == 0 && pMesh->master_wickedmaterialindex > 0) bInstanced = true;
	}
	int iMasterObj = g.entitybankoffset + entid;
	if (ObjectExist(iMasterObj) == 1)
	{
		sObject* pMasterObject = GetObjectData(iMasterObj);
		if (bInstanced)
		{
			WickedSetEntityId(entid);
			WickedSetElementId(0);
			WickedCall_TextureObject(pMasterObject, NULL);
			WickedSetEntityId(-1);
			WickedCall_CopyMaterialColorsToMeshes(pMasterObject);
		}

		// the meshes keep their own copy of the material, which the getters read and the next change writes back, so they
		// take the master's again, as at level load. An instance shares the master's material; a clone (an animated entity)
		// owns one, and the .fpe alone can't restore its base colour, as a white one (4294967295) reads as not set. Not for
		// entities whose meshes are their own (markers, structures, characters made in the creator)
		bool bOwnMeshes = t.entityprofile[entid].ismarker != 0 || t.entityprofile[entid].isebe != 0 || t.entityprofile[entid].ischaractercreator == 1;
		if (bInstanced || (!bOwnMeshes && pObject->iMeshCount == pMasterObject->iMeshCount))
		{
			for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount && iMeshIndex < pMasterObject->iMeshCount; iMeshIndex++)
			{
				sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
				sMesh* pMasterMesh = pMasterObject->ppMeshList[iMeshIndex];
				if (!pMesh || !pMasterMesh) continue;
				if (bInstanced && !(pMesh->bInstanced && pMesh->wickedmaterialindex == 0)) continue;
				pMesh->mMaterial = pMasterMesh->mMaterial;
			}
		}
	}
	entity_applymaterial(entid, e, obj);

	// a base colour the .fpe leaves white is not written to the meshes' own copy, where a colour a script set would linger
	// and come back with the next material change
	WickedCall_CopyMaterialColorsToMeshes(pObject);
}

void entity_prepareobj ( void )
{
	//  takes tte, tobj and tentid
	//  called after entity object clone or instance created (during _entity_createobj and also _entity_converttoclone)
	if (  ObjectExist(t.tobj) == 1 ) 
	{
		//  ensure new object ONLY interacts with main camera and shadow camera
		//  (until postprocess masks kick in)
		if (  t.entityprofile[t.tentid].ismarker != 0 ) 
		{
			SetObjectMask (  t.tobj,1 );
		}
		else
		{
			SetObjectMask (  t.tobj,1+(1<<31) );
		}

		// specific object mask settings
		if ( t.tte > 0 ) visuals_updatespecificobjectmasks ( t.tte, t.tobj );
		SpawnProbe_Sub("masks");

		//  object properties
		if ( 0)//t.entityprofile[t.tentid].ismarker != 0 && t.entityprofile[t.tentid].ismarker != 11 )
		{
			if (t.entityprofile[t.tentid].ismarker == 2)
			{
				SetObjectCull(t.tobj, 1);
			}
		}
		else
		{
			if (t.entityprofile[t.tentid].cullmode >= 0)
			{
				// For Wicked, cull mode controlled per-mesh with parent default as normal
				//PE: Prefer WEMaterial over old cullmode
				bool bUseWEMaterial = false;
				if (t.entityprofile[t.tentid].WEMaterial.MaterialActive)
				{
					WickedSetEntityId(t.tentid);
					if(t.tte > 0)
						WickedSetElementId(t.tte);
					else
						WickedSetElementId(0);
					sObject* pObject = g_ObjectList[t.tobj];
					if (pObject)
					{
						bUseWEMaterial = true;
						for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
						{
							sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
							if (pMesh)
							{
								// set properties of mesh
								WickedSetMeshNumber(iMeshIndex);
								bool bDoubleSided = WickedDoubleSided();
								if (bDoubleSided)
								{
									pMesh->bCull = false;
									pMesh->iCullMode = 0;
									WickedCall_SetMeshCullmode(pMesh);
								}
								else
								{
									pMesh->iCullMode = 1;
									pMesh->bCull = true;
									WickedCall_SetMeshCullmode(pMesh);
								}
							}
						}
					}
					WickedSetEntityId(-1);
					WickedSetElementId(0);
				}
				if (!bUseWEMaterial)
				{
					SetObjectCull(t.tobj, 1);
				}
			}
		}
		SpawnProbe_Sub("cull");

		//  object animation
		entity_resettodefaultanimation ( );

		//  object rotation and scale
		if (  t.entityprofile[t.tentid].fixnewy != 0 ) 
		{
			RotateObject (  t.tobj,0,t.entityprofile[t.tentid].fixnewy,0 );
			FixObjectPivot (  t.tobj );
		}
		if (  t.entityprofile[t.tentid].scale != 0  )  ScaleObject (  t.tobj,t.entityprofile[t.tentid].scale,t.entityprofile[t.tentid].scale,t.entityprofile[t.tentid].scale );
		SpawnProbe_Sub("animation and scale");

		// 091115 - after scaling, ensure LOD is a reflection of overall object size (so LARGE buildings not instantly go to LOD2)
		if (  t.entityprofile[t.tentid].ismarker == 0 ) 
		{
			// 051115 - only if not using limb visibility for hiding decal arrow
			if ( t.entityprofile[t.tentid].addhandlelimb==0 )
			{
				//  set LOD levels for object
				entity_calculateentityLODdistances ( t.tentid, t.tobj, 0 );
			}
		}
		SpawnProbe_Sub("lod distances");

		// no collision and full alpha multiplier
		SetObjectCollisionOff ( t.tobj );
		SpawnProbe_Sub("collision off");
		entity_applymaterial ( t.tentid, t.tte, t.tobj );
		SpawnProbe_Sub("apply material");

		// handle zdepth mode of this entity
		entity_preparedepth(t.tentid, t.tobj);
		SpawnProbe_Sub("depth");

		// apply the scrolls cale uv data values for the shader use later on
		if ( t.entityprofile[t.tentid].uvscrollu != 0.0f 
		||   t.entityprofile[t.tentid].uvscrollv != 0.0f 
		||   t.entityprofile[t.tentid].uvscaleu != 1.0f 
		||   t.entityprofile[t.tentid].uvscalev != 1.0f )
		{
			SetObjectScrollScaleUV ( t.tobj, t.entityprofile[t.tentid].uvscrollu, t.entityprofile[t.tentid].uvscrollv, t.entityprofile[t.tentid].uvscaleu, t.entityprofile[t.tentid].uvscalev );
		}

		// Set art flags for object (can use 32 bit flags here eventually)
		DWORD dwArtFlags = 0;
		if ( t.entityprofile[t.tentid].invertnormal == 1 ) dwArtFlags = 1;
		if ( t.entityprofile[t.tentid].preservetangents == 1 ) dwArtFlags |= 1<<1;
		SetObjectArtFlags ( t.tobj, dwArtFlags, 0.0f );

		//PE: Emulate old classic shaders, using settings.
		cstr sEffectLower = Lower(t.entityprofile[t.tentid].effect_s.Get());
		if (sEffectLower == "effectbank\\reloaded\\decal_animate1_additive.fx")
		{
			//PE: AvengingEagle's Light Effects.
			DisableObjectZWrite(t.tobj); //Additive blending.
			void WickedCall_SetObjectBlendMode(sObject* pObject, int iBlendmode);
			sObject* pObject = g_ObjectList[t.tobj];
			if(pObject)
				WickedCall_SetObjectBlendMode(pObject, BLENDMODE_ADDITIVE);
			t.entityprofile[t.tentid].blendmode = BLENDMODE_ADDITIVE;
			if (t.tte > 0)
				t.entityelement[t.tte].eleprof.blendmode = BLENDMODE_ADDITIVE;
			for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
			{
				if (pObject->ppMeshList[iMesh]) pObject->ppMeshList[iMesh]->iCullMode = 0;
			}
			WickedCall_SetObjectCullmode(pObject);
		}

		if (t.entityprofile[t.tentid].bIsDecal)
		{
			void SetupDecalObject(int obj, int elementID);
			SetupDecalObject(t.tobj, t.tte);
		}
		SpawnProbe_Sub("art flags and effect");


		if (t.entityprofile[t.tentid].ismarker == 0)
		{
			int iAutoFlattenMode = t.entityprofile[t.tentid].autoflatten;
			if (iAutoFlattenMode != 0)
			{
				if (t.entityelement[t.tte].eleprof.iFlattenID != -1)
					entity_updateautoflatten(t.tte, t.tobj);
				else
					entity_autoFlattenWhenAdded(t.tte, t.tobj);
			}
		}
		SpawnProbe_Sub("flatten");
		if (t.entityprofile[t.tentid].ismarker == 2)
		{
			entity_updatelightobj(t.tte, t.tobj);
		}

		//PE: Start any particle effects.
		if (t.entityprofile[t.tentid].ismarker == 10)
		{
			if(t.entityelement[t.tte].obj == 0 )
				t.entityelement[t.tte].obj = t.tobj;
			entity_updateparticleemitter(t.tte);
			//PE: Must always update to org values here, effects are only triggered from LUA now.
			int iParticleEmitter = t.entityelement[t.tte].eleprof.newparticle.emitterid;
			if (iParticleEmitter != -1)
			{
				gpup_setParticleScale(iParticleEmitter, t.entityelement[t.tte].eleprof.newparticle.bParticle_Size);
				gpup_setEffectAnimationSpeed(iParticleEmitter, t.entityelement[t.tte].eleprof.newparticle.fParticle_Speed);
				gpup_setEffectOpacity(iParticleEmitter, t.entityelement[t.tte].eleprof.newparticle.fParticle_Opacity);
			}

		}

		// .fpe noapparentcull = 1: never culled for its apparent size, as characters are, so a small light fixture stays
		// in view from far off; the layer and frustum tests still apply
		if (t.entityprofile[t.tentid].noapparentcull == 1)
		{
			sObject* pObject = g_ObjectList[t.tobj];
			if (pObject) WickedCall_SetObjectPreventAnyApparentOcclusion(pObject, true);
		}
	}
}

void entity_calculateentityLODdistances ( int tentid, int tobj, int iModifier )
{
	float fLODModifier = (100+iModifier)/100.0f;
	if ( t.entityprofile[tentid].lod1distance==0 )
	{
		// default LOD distances a product of scale of object
		float fRelativeScale = ObjectSize ( tobj, 1 ) / 100.0f;
		SetObjectLOD ( tobj, 1, 400.0f * fRelativeScale * fLODModifier );
		SetObjectLOD ( tobj, 2, 800.0f * fRelativeScale * fLODModifier );
	}
	else
	{
		// otherwise its specified by the FPE
		SetObjectLOD ( tobj, 1, (float)t.entityprofile[tentid].lod1distance * fLODModifier );
		SetObjectLOD ( tobj, 2, (float)t.entityprofile[tentid].lod2distance * fLODModifier );
	}
}

void entity_setupcharobjsettings ( void )
{
	// unique extra setup for character objects, takes charanimstate, obj
	if ( t.obj>0 ) 
	{
		if ( ObjectExist(t.obj) == 1 ) 
		{
			PositionObject ( t.obj,t.entityelement[t.charanimstate.e].x,t.entityelement[t.charanimstate.e].y,t.entityelement[t.charanimstate.e].z );
			RotateObject ( t.obj,t.entityelement[t.charanimstate.e].rx,t.entityelement[t.charanimstate.e].ry,t.entityelement[t.charanimstate.e].rz );
			ScaleObject ( t.obj,100+t.entityelement[t.charanimstate.e].scalex,100+t.entityelement[t.charanimstate.e].scaley,100+t.entityelement[t.charanimstate.e].scalez );
		}
	}
}

void entity_resettodefaultanimation ( void )
{
	// takes tte, tobj, tentid
	if ( t.tte != -1 ) 
	{
		bool isWicked = false;
		//PE: Wicked is always clone.
		isWicked = true;
		if ( t.entityelement[t.tte].isclone == 1 || isWicked )
		{
			// CLONE
			if ( GetNumberOfFrames(t.tobj)>0 ) 
			{
				SetObjectFrame ( t.tobj, 0 ); 
				LoopObject ( t.tobj ); 
				StopObject ( t.tobj );
			}
			if ( t.entityelement[t.tte].staticflag == 1 ) 
			{
				// do not animate if marked as static
				//PE: Allow static animation in wicked editor. if set in fpe.
				if (t.entityprofile[t.tentid].animmax > 0 && t.entityprofile[t.tentid].playanimineditor > 0)
				{
					t.q = t.entityprofile[t.tentid].playanimineditor - 1;
					LoopObject(t.tobj, t.entityanim[t.tentid][t.q].start, t.entityanim[t.tentid][t.q].finish);
				}
				else if (t.entityprofile[t.tentid].playanimineditor < 0)
				{
					// uses name instead of index, the negative is the ordinal into the animset
					extern void entity_loop_using_negative_playanimineditor(int e, int obj, cstr animname);
					entity_loop_using_negative_playanimineditor(t.tte, t.tobj, t.entityprofile[t.tentid].playanimineditor_name);
				}
			}
			else
			{		
				if ( t.entityprofile[t.tentid].animmax>0 && t.entityprofile[t.tentid].playanimineditor>0 ) 
				{
					t.q=t.entityprofile[t.tentid].playanimineditor-1;
					LoopObject ( t.tobj,t.entityanim[t.tentid][t.q].start,t.entityanim[t.tentid][t.q].finish );
				}
				else if (t.entityprofile[t.tentid].startanimingame > 0 && t.entityprofile[t.tentid].animmax > 0)
				{
					t.q = t.entityprofile[t.tentid].startanimingame - 1;
					LoopObject(t.tobj, t.entityanim[t.tentid][t.q].start, t.entityanim[t.tentid][t.q].finish);
					StopObject(t.tobj);
				}
				else if (t.entityprofile[t.tentid].playanimineditor < 0)
				{
					// uses name instead of index, the negative is the ordinal into the animset
					extern void entity_loop_using_negative_playanimineditor(int e, int obj, cstr animname);
					entity_loop_using_negative_playanimineditor(t.tte, t.tobj, t.entityprofile[t.tentid].playanimineditor_name);
				}
			}
		}
		else
		{
			// INSTANCE - no self-animation
		}
	}
}

void entity_positionandscale ( void )
{
	// takes tobj,tte,tentid
	PositionObject (  t.tobj,t.entityelement[t.tte].x,t.entityelement[t.tte].y,t.entityelement[t.tte].z );
	RotateObject (  t.tobj,t.entityelement[t.tte].rx,t.entityelement[t.tte].ry,t.entityelement[t.tte].rz );
	ScaleObject (  t.tobj,100+t.entityelement[t.tte].scalex,100+t.entityelement[t.tte].scaley,100+t.entityelement[t.tte].scalez );
	//LB: object is rotated AFTER being created, so ensure particle knows rotation of entity
	if (t.entityprofile[t.entityelement[t.tte].bankindex].ismarker == 10)
		entity_updateparticleemitter(t.tte);

	ShowObject (  t.tobj );

	// ensure all transparent static objects are removed from 'intersect all' consideration
	t.tokay=0;
	if (  t.entityelement[t.tte].staticflag == 1 ) 
	{
		if (  t.entityprofile[t.entityelement[t.tte].bankindex].canseethrough == 1 ) 
		{
			t.tokay=1;
		}
	}
	if (  t.entityprofile[t.entityelement[t.tte].bankindex].ischaracter == 0 ) 
	{
		if (  t.entityprofile[t.entityelement[t.tte].bankindex].collisionmode == 11  )  t.tokay = 1;
	}
	if (  t.tokay == 1 ) 
	{
		SetObjectCollisionProperty (  t.tobj,1 );
	}
	return;
}


#define STOPPROBELIGHTLEAK

XMFLOAT3 Hit[6];
float RayAroundObject(int obj,float x, float y, float z)
{
	if (obj <= 0.0) return 0.0f;
	if (!ObjectExist(obj)) return 0.0f;

	sObject* pObject = g_ObjectList[obj];
	if (!pObject) return 0.0f;
	
	int iVisible = GetVisible(obj);
	HideObject(obj);

	XMFLOAT3 ObjectCenter = { 0,0,0 }; //GetRealCenterToGridEntity();
	XMFLOAT3 ObjectSize = { 0,0,0 };
	XMFLOAT3 fObjCenter = { x,y,z };
	float fLowetDist = 99999.0f;
	Hit[0] = { 0,0,0 }; // <-  
	Hit[1] = { 0,0,0 }; // /|\  
	Hit[2] = { 0,0,0 }; // ->  
	Hit[3] = { 0,0,0 }; // \|/  
	Hit[4] = { 0,0,0 }; //  / 
	Hit[5] = { 0,0,0 }; //  \  

	for (int i = 0; i < 6; i++)
	{
		if (i == 0)
		{
			ObjectSize.x = (pObject->collision.vecMin.x - pObject->collision.vecMax.x) * 0.5;  //offset from center.
			ObjectSize.y = pObject->collision.vecMin.y;
			ObjectSize.z = 0.0f;
		}
		else if (i == 1)
		{
			ObjectSize.x = 0.0f;
			ObjectSize.y = pObject->collision.vecMin.y;
			ObjectSize.z = (pObject->collision.vecMin.z - pObject->collision.vecMax.z) * 0.5; //offset from center.
		}
		else if (i == 2)
		{
			ObjectSize.x = fabs((pObject->collision.vecMin.x - pObject->collision.vecMax.x) * 0.5);  //offset from center.
			ObjectSize.y = pObject->collision.vecMin.y;
			ObjectSize.z = 0.0f;
		}
		else if (i == 3)
		{
			ObjectSize.x = 0.0f;
			ObjectSize.y = pObject->collision.vecMin.y;
			ObjectSize.z = fabs((pObject->collision.vecMin.z - pObject->collision.vecMax.z) * 0.5); //offset from center.
		}
		else if (i == 4)
		{
			ObjectSize.x = (pObject->collision.vecMin.x - pObject->collision.vecMax.x) * 0.5;  //offset from center.
			ObjectSize.y = pObject->collision.vecMin.y;
			ObjectSize.z = (pObject->collision.vecMin.z - pObject->collision.vecMax.z) * 0.5; //offset from center.
		}
		else if (i == 5)
		{
			ObjectSize.x = fabs((pObject->collision.vecMin.x - pObject->collision.vecMax.x) * 0.5);  //offset from center.
			ObjectSize.y = pObject->collision.vecMin.y;
			ObjectSize.z = fabs((pObject->collision.vecMin.z - pObject->collision.vecMax.z) * 0.5); //offset from center.
		}

		XMFLOAT3 fObjPos = { x + ObjectCenter.x + (ObjectSize.x) , y + ObjectCenter.y ,z + ObjectCenter.z + (ObjectSize.z) };
		XMFLOAT3 fObjRayPos = { x + ObjectCenter.x - (ObjectSize.x) , y + ObjectCenter.y ,z + ObjectCenter.z - (ObjectSize.z) };
		XMVECTOR vectorSub = XMVectorSubtract(XMLoadFloat3(&fObjPos), XMLoadFloat3(&fObjCenter));
		XMVECTOR rayDirection = XMVector3Normalize(vectorSub);
		XMFLOAT3 f3Dir;
		XMStoreFloat3(&f3Dir, rayDirection);
		float fHitX, fHitY, fHitZ, fdist = 99999.0;
		if (WickedCall_SentRay(fObjRayPos.x, fObjRayPos.y, fObjRayPos.z, f3Dir.x, 0.0f, f3Dir.z, &fHitX, &fHitY, &fHitZ, NULL, NULL, NULL, NULL, GGRENDERLAYERS_NORMAL))
		{
			XMFLOAT3 fObjHit = { fHitX, fHitY, fHitZ };

			float fdist = sqrt((fObjPos.x - fObjHit.x) * (fObjPos.x - fObjHit.x) +
				(fObjPos.z - fObjHit.z) * (fObjPos.z - fObjHit.z));
			if (fdist < fLowetDist)
				fLowetDist = fdist;

			Hit[i] = fObjHit;
		}
	}
	if (iVisible) ShowObject(obj);
	if (fLowetDist < 99990.0f)
		return fLowetDist;
	return(0.0f);
}

void entity_placeprobe(int obj, float fLightProbeScale)
{
	return; // don't place static probes 

	// NOTE: Can only have 16 probes before lighting goes nuts - find out why!
	if (obj <= 0) return;
	sObject* pObject = g_ObjectList[obj];

	// work out object center
	XMFLOAT3 fObjCenter;
	if (pObject)
	{
		// position of probe relative to light object (nice if we could HIDE the light when env maps being created (for editor))
		float fShiftDownToMissLightObject = -20.0f;
		fObjCenter = { pObject->position.vecPosition.x, pObject->position.vecPosition.y + fShiftDownToMissLightObject, pObject->position.vecPosition.z };
	}

	float fSize = 180.0f;

	//PE: Probe create light leak, when they are bigger then a room and reach to the other side.
	//PE: try to raycast around light to find sides and center at that point.
	#ifdef STOPPROBELIGHTLEAK
	if (pObject)
	{
		fSize = RayAroundObject(obj, pObject->position.vecPosition.x, pObject->position.vecPosition.y, pObject->position.vecPosition.z);
		if (fSize > 0)
		{
			//Try a new center.
			if (Hit[0].x > 0 && Hit[2].x > 0)
			{
				XMVECTOR vectorSub = XMVectorSubtract(XMLoadFloat3(&Hit[2]), XMLoadFloat3(&Hit[0]));
				XMFLOAT3 fObjOffset;
				fObjOffset.x = XMVectorGetX(vectorSub) * 0.5;
				fObjOffset.y = XMVectorGetY(vectorSub) * 0.5;
				fObjOffset.z = XMVectorGetZ(vectorSub) * 0.5;

				float fSize2 = RayAroundObject(obj, Hit[0].x + fObjOffset.x, pObject->position.vecPosition.y, Hit[0].z-fObjOffset.z);
				if (fSize2 > 0 && fSize2 > fSize)
				{
					fSize = fSize2;
					fObjCenter = { Hit[0].x + fObjOffset.x, pObject->position.vecPosition.y, Hit[0].z - fObjOffset.z };
				}
			}
			if (Hit[1].z > 0 && Hit[3].z > 0)
			{
				XMVECTOR vectorSub = XMVectorSubtract(XMLoadFloat3(&Hit[3]), XMLoadFloat3(&Hit[1]));
				XMFLOAT3 fObjOffset;
				fObjOffset.x = XMVectorGetX(vectorSub) * 0.5;
				fObjOffset.y = XMVectorGetY(vectorSub) * 0.5;
				fObjOffset.z = XMVectorGetZ(vectorSub) * 0.5;

				float fSize2 = RayAroundObject(obj, Hit[0].x + fObjOffset.x, pObject->position.vecPosition.y, Hit[0].z - fObjOffset.z);
				if (fSize2 > 0 && fSize2 > fSize)
				{
					fSize = fSize2;
					fObjCenter = { Hit[0].x + fObjOffset.x, pObject->position.vecPosition.y, Hit[0].z - fObjOffset.z };
				}
			}

			if (Hit[3].z > 0 && Hit[1].z == 0)
			{
				//Try moving away from the wall and see if we get better results.
				XMFLOAT3 fObjOffset;
				fObjOffset.x = 0;
				fObjOffset.y = 0;
				fObjOffset.z = 30;
				float fSize2 = RayAroundObject(obj, fObjCenter.x + fObjOffset.x, pObject->position.vecPosition.y, fObjCenter.z - fObjOffset.z);
				if (fSize2 > 0 && fSize2 > fSize)
				{
					fSize = fSize2;
					fObjCenter = { fObjCenter.x + fObjOffset.x, pObject->position.vecPosition.y, fObjCenter.z - fObjOffset.z };
				}
				fObjOffset.x = 0;
				fObjOffset.y = 0;
				fObjOffset.z = 60.0f; //Try fixed 100 offset.
				fSize2 = RayAroundObject(obj, fObjCenter.x + fObjOffset.x, pObject->position.vecPosition.y, fObjCenter.z - fObjOffset.z);
				if (fSize2 > 0 && fSize2 > fSize)
				{
					fSize = fSize2;
					fObjCenter = { fObjCenter.x + fObjOffset.x, pObject->position.vecPosition.y, fObjCenter.z - fObjOffset.z };
				}
			}
		}
		//PE: If walls are rotate around probe, we cant currently rotate the box , so tak out a little.
		if (fSize > 50.0) fSize -= 7.0;
		if (fSize <= 0 || fSize > 180.0f)
		{
			fSize = 180.0f;
		}
	}
	#endif

	if (pObject)
	{
		// LB: modulate size of prove with scale passed in (controlled in light properties)
		fSize *= fLightProbeScale;

		//PE: Add remove if already exists.
		cStr name = cStr(obj);
		WickedCall_CreateReflectionProbe(fObjCenter.x, fObjCenter.y, fObjCenter.z, name.Get(), fSize);
	}
}

void entity_deleteprobe(int obj)
{
	if (obj <= 0) return;
	sObject* pObject = g_ObjectList[obj];
	if (pObject)
	{
		cStr name = cStr(obj);
		WickedCall_DeleteReflectionProbe(name.Get());
	}
}

void entity_updateentityobj ( void )
{
	//  special mode which intercepts non-static entities and replaces with blanks
	if ( t.lightmapper.onlyloadstaticentitiesduringlightmapper == 1 ) 
	{
		//  eliminate entities that NEVER get lightmapped
		if (  t.entityelement[t.tupdatee].staticflag == 0 ) 
		{
			t.entityelement[t.tupdatee].bankindex=0;
			t.entityelement[t.tupdatee].obj=0;
		}
	}

	// Create/replace/remove olay object to reflect (olayindex,ti)
	t.tentid=t.entityelement[t.tupdatee].bankindex;
	if (  t.tentid != 0 ) 
	{
		t.obj=t.entityelement[t.tupdatee].obj;
		if (  t.obj>0 ) 
		{
			if (  ObjectExist(t.obj) == 1  )  DeleteObject (  t.obj );
			t.entityelement[t.tupdatee].obj=0;

			//LB: this ensures the deleted OBJ ID can be reused immediately so they entity element OBJ ID is the same when refreshed (modifying media while running level)
			g.entityviewcurrentobj = t.obj;
		}
		if (  t.entityelement[t.tupdatee].obj == 0 ) 
		{
			//  find free object
			t.obj=g.entityviewcurrentobj;
			if (  ObjectExist(t.obj) == 1 ) 
			{
				while ( ObjectExist(t.obj)==1 ) ++t.obj;
				g.entityviewcurrentobj=t.obj;
			}
			if (  g.entityviewcurrentobj>g.entityviewendobj ) 
			{
				g.entityviewendobj=g.entityviewcurrentobj;
			}
			g.editorresourcecounterpacer=1;

			//  create object
			entity_createobj ( );
			t.tobj=t.obj ; t.tte=t.tupdatee ; entity_positionandscale ( );
			t.entityelement[t.tupdatee].obj=t.obj;
		}
	}
	if ( t.tentid == 0 ) 
	{
		t.obj=t.entityelement[t.tupdatee].obj;
		if ( t.obj>0 ) 
		{
			if ( ObjectExist(t.obj) == 1 ) DeleteObject ( t.obj );
		}
		t.entityelement[t.tupdatee].obj=0;
	}
}
