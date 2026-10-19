uint32_t WickedCall_LoadWiSceneDirect(Scene& scene2, char* filename, bool attached, char* changename, char* changenameto);
bool preload_wicked_particle_effect(newparticletype* pParticle, int decal_id)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	// disable wicked particles (for testing/etc)
	extern int g_iDisableWParticleSystem;
	if (g_iDisableWParticleSystem == 1)
	{
		return false;
	}

	//PE: Preload effects so there is no delays.
	int MaxCachedDecals = MAXREADYDECALS;
	if (pParticle->iMaxCache > 0 && pParticle->iMaxCache < MAXREADYDECALS)
		MaxCachedDecals = pParticle->iMaxCache;

	int iParticleEmitter = pParticle->emitterid;
	if (iParticleEmitter == -1)
	{
		if (pParticle->bWPE)
		{
			Scene& scene = wiScene::GetScene();
			uint32_t master_root = 0;
			for (int i = 0; i < MaxCachedDecals; i++)
			{
				if (decal_id >= 1 && decal_id < MAXUNIQUEDECALS && ready_decals[decal_id][i] == 0)
				{
					uint32_t root = 0;
					Entity new_root = 0;
					uint32_t count_before = scene.emitters.GetCount();
					uint32_t mat_count_before = scene.materials.GetCount();

					char path[MAX_PATH];
					strcpy(path, pParticle->emittername.Get());
					GG_GetRealPath(path, 0);

					if (master_root > 0)
					{
						new_root = GetScene().Entity_Duplicate(master_root);
					}
					else
					{
						WickedCall_LoadWiScene(path, false, NULL, NULL);
					}

					uint32_t count_after = scene.emitters.GetCount();
					if (count_before != count_after)
					{
						Entity emitter = scene.emitters.GetEntity(scene.emitters.GetCount() - 1);
						Entity matemitter = scene.materials.GetEntity(scene.materials.GetCount() - 1);
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
							//ec->Restart();
							//ec->SetVisible(true); // REMOVED
						}

						if (master_root > 0)
						{
							//PE: resource sometimes empty when using Entity_Duplicate.
							int from = mat_count_before;
							int to = scene.materials.GetCount();
							for (; from < to; from++)
							{
								for (int a = 0; a < MaterialComponent::EMISSIVEMAP; a++)
								{
									if (scene.materials[from].textures[a].name.size() > 0)
									{
										if (!scene.materials[from].textures[a].resource.IsValid())
										{
											scene.materials[from].textures[a].resource = WickedCall_LoadImage(scene.materials[from].textures[a].name);
										}
									}
								}
								scene.materials[from].SetDirty();
							}
						}

					}
					if (root != 0)
					{
						if (decal_id >= 1 && decal_id < MAXUNIQUEDECALS && ready_decals[decal_id][i] == 0)
						{
							iParticleEmitter = pParticle->emitterid = root;
							ready_decals[decal_id][i] = root;
						}
						else
						{
							iParticleEmitter = pParticle->emitterid = root;
						}
						if (master_root == 0)
						{
							master_root = root;
							//PE: Validate that decal is a burst only emitter.
							for (int b = 0; b < scene.emitters.GetCount(); b++)
							{
								Entity emitter = scene.emitters.GetEntity(b);
								HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
								if (hier)
								{
									if (hier->parentID == master_root)
									{
										wiEmittedParticle* ec = scene.emitters.GetComponent(emitter);
										if (ec->count > 0.1f)
										{
											//PE: This is not a burst emitter reject it.
											iParticleEmitter = pParticle->emitterid = -1;
											ready_decals[decal_id][i] = 0;

											WickedCall_PerformEmitterAction(2, root); //PE: Pause
											WickedCall_PerformEmitterAction(6, root); //PE: Not Visible
											void DeleteEmitterEffects(uint32_t root);
											DeleteEmitterEffects(master_root);
											return false;
										}
									}
								}
							}
						}

						WickedCall_PerformEmitterAction(2, root); //PE: Pause
						WickedCall_PerformEmitterAction(6, root); //PE: Not Visible
					}
				}
			}

			// GGMAX 2026-08-04 (WPE Phase 0a): if nothing was created, say so.
			// Returning true here left the caller with bWPE still set and emitterid -1, which
			// skips BOTH the WPE draw and the legacy .arx fallback in decal_load() - so a decal
			// whose wpe.pe failed to load rendered no effect of either kind. The "false means
			// fall back to legacy" contract already exists (see the burst-only rejection above),
			// this just honours it for the load-failed case too.
			if (pParticle->emitterid == -1)
				return false;
		}
	}
	return true;
}

// GG: pvecUp, when given, is the up a fired WPE effect stands on (a projectile's explosion on the surface it hit)
void newparticle_updateparticleemitter ( newparticletype* pParticle, float fScale, float fX, float fY, float fZ, float fRX, float fRY, float fRZ, GGMATRIX* pmatBaseRotation,bool bAutoDelete,int decal_id, const GGVECTOR3* pvecUp = NULL)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	// show or hide based on editor vs test game
	bool bShowThisParticle = false;
	extern bool bImGuiInTestGame;
	if (bImGuiInTestGame == true)
		bShowThisParticle = pParticle->bParticle_Show_At_Start;
	else
		bShowThisParticle = pParticle->bParticle_Preview;

	int iParticleEmitter = pParticle->emitterid;
	if (iParticleEmitter == -1)
	{
		if (pParticle->bWPE)
		{
			int MaxCachedDecals = MAXREADYDECALS;
			if (pParticle->iMaxCache > 0 && pParticle->iMaxCache < MAXREADYDECALS)
				MaxCachedDecals = pParticle->iMaxCache;

			Scene& scene = wiScene::GetScene();
			if (bShowThisParticle == true)
			{
				//PE: Use Cached effects.
				if (decal_id >= 1 && decal_id < MAXUNIQUEDECALS && ready_decals[decal_id][decal_count[decal_id]] > 0)
				{
					iParticleEmitter = pParticle->emitterid = ready_decals[decal_id][decal_count[decal_id]];
					decal_count[decal_id]++;
					if (decal_count[decal_id] >= MaxCachedDecals)
						decal_count[decal_id] = 0;
				}
				else
				{
					uint32_t root = 0;
					uint32_t count_before = scene.emitters.GetCount();

					char path[MAX_PATH];
					strcpy(path, pParticle->emittername.Get());
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
							//ec->SetVisible(true); // REMOVED
						}
					}
					if (root != 0)
					{
						if (decal_id >= 1 && decal_id < MAXUNIQUEDECALS && ready_decals[decal_id][decal_count[decal_id]] == 0)
						{
							iParticleEmitter = pParticle->emitterid = root;
							ready_decals[decal_id][decal_count[decal_id]] = root;
						}
						else
						{
							iParticleEmitter = pParticle->emitterid = root;
							if (bAutoDelete)
								delete_decal_particles.push_back(iParticleEmitter);
						}
						//PE: Not cached effects (not active at start) only use 2 cached versions.
						decal_count[decal_id]++;
						if (decal_count[decal_id] >= MaxCachedDecals)
							decal_count[decal_id] = 0;
					}
				}
			}
		}
		else
		if (bShowThisParticle == true)
		{
			iParticleEmitter = gpup_loadEffect(pParticle->emittername.Get(), 0, 0, 0, 1.0);
			gpup_emitterActive(iParticleEmitter, 0);
			pParticle->emitterid = iParticleEmitter;
			//PE: Anything created in testgame (LUA) must be deleted after game.
			if(bAutoDelete || bImGuiInTestGame == true)
				delete_decal_particles.push_back(iParticleEmitter);
		}
	}
	if (iParticleEmitter != -1)
	{
		/*
		if (pParticle->bWPE)
		{
			Scene& scene = wiScene::GetScene();
			TransformComponent* root_tranform = scene.transforms.GetComponent(iParticleEmitter);
			if (bShowThisParticle)
			{
				if (pParticle->bParticle_Fire == true)
				{
					if (root_tranform)
					{
						root_tranform->ClearTransform();
						root_tranform->Translate(XMFLOAT3(fX, fY, fZ));
						root_tranform->UpdateTransform();
					}
					WickedCall_PerformEmitterAction(3, iParticleEmitter); //PE: Resume
					// GGMAX 2.29 (2026-08-12): RESUME EMISSION. Action 3 is SetPaused(false) and
					// action 8 is SetEmitPaused(false) — two DIFFERENT flags, and neither Restart()
					// nor Burst() clears FLAG_EMIT_PAUSE (wiEmittedParticle.h:204; only an explicit
					// SetEmitPaused(false) does). Before 2.29 nothing in the game ever issued action
					// 8 except the weapon-trail respawn (M-Weapon.cpp:1372), so the moment 2.28
					// started emit-pausing expired decal emitters (M-Decal.cpp:996) the pause was
					// permanent: after MAXREADYDECALS (5) impacts of one decal type, every cached
					// emitter of that type was emit-paused and the effect stopped appearing.
					// Reported from live play. Fire must clear BOTH pause flags — this is the
					// same 7/8 pairing the weapon-trail path has always used.
					WickedCall_PerformEmitterAction(8, iParticleEmitter); //PE: Resume emit
					WickedCall_PerformEmitterAction(4, iParticleEmitter); //PE: Restart
					WickedCall_PerformEmitterAction(5, iParticleEmitter); //PE: Visible
					WickedCall_PerformEmitterAction(1, iParticleEmitter); //PE: Burst All
					pParticle->bParticle_Fire = false;
				}
			}
		}
		*/

		//PE: Make emitter always face camera.
		if (pParticle->bWPE)
		{
			Scene& scene = wiScene::GetScene();
			TransformComponent* root_tranform = scene.transforms.GetComponent(iParticleEmitter);

			if (bShowThisParticle)
			{
				if (pParticle->bParticle_Fire == true)
				{
					if (root_tranform)
					{
						root_tranform->ClearTransform();
						XMVECTOR cameraPos = XMVectorSet(CameraPositionX(0), CameraPositionY(0), CameraPositionZ(0), 0.0f);
						XMVECTOR emitterPos = XMVectorSet(fX, fY, fZ, 0.0f);

						XMVECTOR lookDir = XMVectorSubtract(cameraPos, emitterPos);
						//PE: Invert drection - XMVECTOR lookDir = XMVectorSubtract(emitterPos, cameraPos);
						lookDir = XMVectorSetY(lookDir, 0.0f);

						// GG: stood on a surface, its Y is the surface's normal; on a floor or ceiling it still faces the
						// camera, on a wall world up keeps it from turning about the normal
						XMVECTOR surfaceUp = pvecUp ? XMVectorSet(pvecUp->x, pvecUp->y, pvecUp->z, 0.0f) : XMVectorZero();
						if (XMVectorGetX(XMVector3LengthSq(surfaceUp)) > 0.25f)
						{
							surfaceUp = XMVector3Normalize(surfaceUp);
							XMVECTOR facing = fabs(XMVectorGetY(surfaceUp)) > 0.7f ? lookDir : XMVectorSet(0, 1, 0, 0);
							facing = XMVectorSubtract(facing, XMVectorScale(surfaceUp, XMVectorGetX(XMVector3Dot(facing, surfaceUp))));
							if (XMVectorGetX(XMVector3LengthSq(facing)) < 0.0001f)
							{
								facing = XMVector3Cross(surfaceUp, XMVectorSet(1, 0, 0, 0));
								if (XMVectorGetX(XMVector3LengthSq(facing)) < 0.0001f) facing = XMVector3Cross(surfaceUp, XMVectorSet(0, 0, 1, 0));
							}
							facing = XMVector3Normalize(facing);
							XMMATRIX lookAtMat = XMMatrixLookToLH(emitterPos, facing, surfaceUp);
							root_tranform->MatrixTransform(XMMatrixInverse(nullptr, lookAtMat));
						}
						else if (XMVectorGetX(XMVector3LengthSq(lookDir)) > 0.0001f)
						{
							lookDir = XMVector3Normalize(lookDir);
							XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
							XMMATRIX lookAtMat = XMMatrixLookToLH(emitterPos, lookDir, worldUp);
							XMMATRIX worldMat = XMMatrixInverse(nullptr, lookAtMat);
							root_tranform->MatrixTransform(worldMat);
						}
						else
						{
							root_tranform->Translate(XMFLOAT3(fX, fY, fZ));
						}

						root_tranform->UpdateTransform();
					}

					WickedCall_PerformEmitterAction(3, iParticleEmitter); // Resume
					WickedCall_PerformEmitterAction(4, iParticleEmitter); // Restart
					WickedCall_PerformEmitterAction(5, iParticleEmitter); // Visible
					WickedCall_PerformEmitterAction(1, iParticleEmitter); // Burst All
					pParticle->bParticle_Fire = false;
				}
			}
		}
		else
		{
			// set emitter position, rotation and scale
			gpup_setGlobalPosition(iParticleEmitter, fX, fY, fZ);
			if (pParticle->bParticle_Offset_Used == true)
			{
				float x = pParticle->bParticle_Offset_X;
				float y = pParticle->bParticle_Offset_Y;
				float z = pParticle->bParticle_Offset_Z;
				gpup_setLocalPosition(iParticleEmitter, x, y, z);
			}
			else
			{
				gpup_resetLocalPosition(iParticleEmitter);
			}
			float fSpeedX, fSpeedY, fSpeedZ;
			gpup_getEmitterSpeedAngleAdjustment(iParticleEmitter, &fSpeedX, &fSpeedY, &fSpeedZ);
			GGVECTOR3 vecSpeedDirection = GGVECTOR3(fSpeedX - 0.5f, fSpeedY - 0.5f, fSpeedZ - 0.5f);
			if (pParticle->bParticle_LocalRot_Used == true)
			{
				// local emitter rotation
				float x = GGToRadian(pParticle->bParticle_LocalRot_X);
				float y = GGToRadian(pParticle->bParticle_LocalRot_Y);
				float z = GGToRadian(pParticle->bParticle_LocalRot_Z);
				GGMATRIX matLocalRot;
				GGMatrixRotationYawPitchRoll(&matLocalRot, y, x, z);
				GGVec3TransformCoord(&vecSpeedDirection, &vecSpeedDirection, &matLocalRot);
				if (pmatBaseRotation)
				{
					// global rotation
					GGVec3TransformCoord(&vecSpeedDirection, &vecSpeedDirection, pmatBaseRotation);
				}
				gpup_setEmitterSpeedAngleAdjustment(iParticleEmitter, 0.5f + vecSpeedDirection.x, 0.5f + vecSpeedDirection.y, 0.5f + vecSpeedDirection.z);
			}
			gpup_setGlobalRotation(iParticleEmitter, fRX, fRY, fRZ);
			gpup_setGlobalScale(iParticleEmitter, 100.0f + fScale);

			// set whether burst mode loops
			if (pParticle->bParticle_Looping_Animation == true)
				gpup_emitterBurstMode(iParticleEmitter, 0);
			else
				gpup_emitterBurstMode(iParticleEmitter, 1);

			// switch emitter on or off
			if (bShowThisParticle == true)
				gpup_emitterActive(iParticleEmitter, 1);
			else
				gpup_emitterActive(iParticleEmitter, 0);

			// specify psrticle speed
			if (pParticle->bParticle_SpeedChange == true)
			{
				if (pParticle->fParticle_Speed_Original == -123.0f) pParticle->fParticle_Speed_Original = gpup_getParticleSpeed(iParticleEmitter);
				gpup_setEffectAnimationSpeed(iParticleEmitter, pParticle->fParticle_Speed);
			}
			else
			{
				if (pParticle->fParticle_Speed_Original != -123.0f)
				{
					gpup_setEffectAnimationSpeed(iParticleEmitter, pParticle->fParticle_Speed_Original);
				}
			}

			// specify psrticle opacity
			if (pParticle->bParticle_OpacityChange == true)
			{
				if (pParticle->fParticle_Opacity_Original == -123.0f) pParticle->fParticle_Opacity_Original = gpup_getParticleOpacity(iParticleEmitter);
				gpup_setEffectOpacity(iParticleEmitter, pParticle->fParticle_Opacity);
			}
			else
			{
				if (pParticle->fParticle_Opacity_Original != -123.0f)
				{
					gpup_setEffectOpacity(iParticleEmitter, pParticle->fParticle_Opacity_Original);
				}
			}

			// specify particle size
			if (pParticle->bParticle_SizeChange == true)
			{
				if (pParticle->bParticle_Size_Original == -123.0f) pParticle->bParticle_Size_Original = gpup_getParticleSize(iParticleEmitter);
				gpup_setParticleScale(iParticleEmitter, pParticle->bParticle_Size);
			}
			else
			{
				if (pParticle->bParticle_Size_Original != -123.0f)
				{
					gpup_setParticleScale(iParticleEmitter, pParticle->bParticle_Size_Original);
				}
			}

			// handle any triggering of a fire burst
			if (pParticle->bParticle_Fire == true)
			{
				gpup_emitterFire(iParticleEmitter);
				pParticle->bParticle_Fire = false;
			}

			// handle particle collisions with floor and sphere (for reflection bounce)
			if (pParticle->fParticle_BouncinessChange == true)
			{
				if (pParticle->fParticle_Bounciness_Original == -123.0f) pParticle->fParticle_Bounciness_Original = gpup_getBounciness(iParticleEmitter) * 5.0f;
				gpup_setBounciness(iParticleEmitter, pParticle->fParticle_Bounciness / 5.0f);
			}
			else
			{
				if (pParticle->fParticle_Bounciness_Original != -123.0f)
				{
					gpup_setBounciness(iParticleEmitter, pParticle->fParticle_Bounciness_Original / 5.0f);
				}
			}
			if (pParticle->iParticle_Floor_Active > 0)
			{
				if (pParticle->fParticle_Floor_Height_Original == -123.0f) pParticle->fParticle_Floor_Height_Original = gpup_getFloorReflectionHeight(iParticleEmitter);
				gpup_floorReflection(iParticleEmitter, pParticle->iParticle_Floor_Active - 1, pParticle->fParticle_Floor_Height);
			}
			else
			{
				if (pParticle->fParticle_Floor_Height_Original != -123.0f)
				{
					gpup_restoreFloorReflection(iParticleEmitter, 1, pParticle->fParticle_Floor_Height_Original);
				}
			}

			// handle color if particle effect
			if (pParticle->bParticle_ColorChange == true)
			{
				if (pParticle->fParticle_R_Original == -123.0f) gpup_getEffectColor(iParticleEmitter, &pParticle->fParticle_R_Original, &pParticle->fParticle_G_Original, &pParticle->fParticle_B_Original);
				gpup_setEffectColor(iParticleEmitter, pParticle->fParticle_R, pParticle->fParticle_G, pParticle->fParticle_B);
			}
			else
			{
				if (pParticle->fParticle_R_Original != -123.0f)
				{
					gpup_setEffectColor(iParticleEmitter, pParticle->fParticle_R_Original, pParticle->fParticle_G_Original, pParticle->fParticle_B_Original);
				}
			}

			// handle change of lifespan
			if (pParticle->bParticle_LifespanChange == true)
			{
				if (pParticle->fParticle_Lifespan_Original == -123.0f) pParticle->fParticle_Lifespan_Original = gpup_getEffectLifespan(iParticleEmitter);
				gpup_setEffectLifespan(iParticleEmitter, pParticle->fParticle_Lifespan);
			}
			else
			{
				if (pParticle->fParticle_Lifespan_Original != -123.0f)
				{
					gpup_setEffectLifespan(iParticleEmitter, pParticle->fParticle_Lifespan_Original);
				}
			}
		}
	}
}

void newparticle_deleteparticleemitter( int iParticleEffect )
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	delete_decal_particles.erase(std::remove(delete_decal_particles.begin(), delete_decal_particles.end(), iParticleEffect), delete_decal_particles.end());
	gpup_deleteEffect(iParticleEffect);
}

void entity_updateparticleemitterbyID ( entityeleproftype* pEleprof, int iObj, float fScale, float fX, float fY, float fZ, float fRX, float fRY, float fRZ)
{
	// get base rotation of object (for speed vector control)
	GGMATRIX* pmatBaseRotation = NULL;
	sObject* pObject = GetObjectData(iObj);
	if (pObject) pmatBaseRotation = &pObject->position.matRotation;

	// control particle settings via ptr
	newparticle_updateparticleemitter(&pEleprof->newparticle, fScale, fX, fY, fZ, fRX, fRY, fRZ, pmatBaseRotation,false, -1);
}

void entity_updateparticleemitter ( int e )
{
	// also handle particle emitter entity
	if (e > 0)
	{
		if (t.entityprofile[t.entityelement[e].bankindex].ismarker == 10)
		{
			entity_updateparticleemitterbyID(&t.entityelement[e].eleprof, t.entityelement[e].obj, t.entityelement[e].scalex, t.entityelement[e].x, t.entityelement[e].y, t.entityelement[e].z, t.entityelement[e].rx, t.entityelement[e].ry, t.entityelement[e].rz);
		}
	}
}

// the size a flatten pad passes as sizeX: a rect (autoflatten 1) uses the footprint width; a circle (any other value)
// the larger footprint side, so creating and updating a round pad give it the same diameter (it was created from the
// bounding-sphere radius, which includes the height, and updated from the width)
static float entity_autoflattenpadsizex (int iAutoFlattenMode, float sx, float sz)
{
	if (iAutoFlattenMode == 1) return sx;
	return (sx > sz) ? sx : sz;
}

// whether entity e clears the trees and grass under it without flattening: its Clear Grass and Trees tick once set in
// the editor, 1 yes, 2 no; until then (0, also every placement saved before the tick existed) clearvegetation in its
// .fpe, and without that key static objects at least 3m across their narrower side and 2.5m tall, so a building clears
// them and a small prop or a parked car does not. Auto flatten, when on for the entity, clears them with its pad instead
bool entity_clearsvegetation(int e)
{
	int entid = t.entityelement[e].bankindex;
	if (entid <= 0) return false;
	if (t.entityelement[e].iClearVegetation == 1) return true;
	if (t.entityelement[e].iClearVegetation == 2) return false;
	int iClearVegetation = t.entityprofile[entid].clearvegetation;
	if (iClearVegetation != -1) return iClearVegetation == 1;
	if (t.entityelement[e].staticflag != 1) return false;
	if (t.entityprofile[entid].ismarker != 0 || t.entityprofile[entid].ischaracter != 0 || t.entityprofile[entid].bIsDecal) return false;
	int iObj = t.entityelement[e].obj;
	if (iObj <= 0 || ObjectExist(iObj) == 0) return false;
	float fNarrowerSide = ObjectSizeX(iObj, 1);
	if (ObjectSizeZ(iObj, 1) < fNarrowerSide) fNarrowerSide = ObjectSizeZ(iObj, 1);
	return fNarrowerSide >= GGTerrain_MetersToUnits(3.0f) && ObjectSizeY(iObj, 1) >= GGTerrain_MetersToUnits(2.5f);
}

// keeps the Clear Grass and Trees area (a vegetation area, which leaves the terrain height alone) of an entity that
// clears vegetation while auto flatten is off for it. It shares eleprof.iFlattenID with the auto flatten pad, as an
// entity has one or the other, so every place that removes a pad removes it too. Returns true when it has dealt with
// the entity, false to leave it to the auto flatten code. In game the areas stay as the editor left them
static bool entity_updatevegetationarea(int e, int obj)
{
	int entid = t.entityelement[e].bankindex;
	if (entid <= 0) return false;
	int iFlattenID = t.entityelement[e].eleprof.iFlattenID;
	bool bHasVegetationArea = (iFlattenID > 0 && GGTerrain_IsVegetationArea(iFlattenID) == 1);
	if (g.isGameBeingPlayed) return bHasVegetationArea;

	bool bAutoFlatten = (t.entityprofile[entid].autoflatten != 0 && g_bEnableAutoFlattenSystem && t.entityelement[e].eleprof.bAutoFlatten);
	if (bAutoFlatten)
	{
		if (!bHasVegetationArea) return false;
		// auto flatten was turned on: its pad takes the place of the vegetation area
		GGTerrain_RemoveFlatArea(iFlattenID);
		t.entityelement[e].eleprof.iFlattenID = -1;
		entity_autoFlattenWhenAdded(e, obj);
		return true;
	}

	// only once the entity's own object is in place: while it is being created (entity_prepareobj) its object does not
	// have the entity's rotation and scale yet
	int iObj = t.entityelement[e].obj;
	if (iObj <= 0 || ObjectExist(iObj) == 0) return bHasVegetationArea;

	bool bClears = entity_clearsvegetation(e);
	extern std::vector<int> g_smartObjectDummyEntities;
	if (t.entityelement[e].iIsSmarkobjectDummyObj == 1) bClears = false;
	for (auto& dummyID : g_smartObjectDummyEntities) if (dummyID == e) bClears = false;
	if (t.entityelement[e].x == 0 && t.entityelement[e].y == -500000 && t.entityelement[e].z == 0) bClears = false;
	if (!bClears)
	{
		if (!bHasVegetationArea) return false;
		GGTerrain_RemoveFlatArea(iFlattenID);
		t.entityelement[e].eleprof.iFlattenID = -1;
		return true;
	}

	if (iFlattenID != -1 && !bHasVegetationArea)
	{
		// a pad left from when auto flatten was on
		GGTerrain_RemoveFlatArea(iFlattenID);
		iFlattenID = -1;
	}

	// the object's footprint around its collision centre, turned to its yaw
	GGQUATERNION QuatAroundX, QuatAroundY, QuatAroundZ, quatRotationEvent;
	GGQuaternionRotationAxis(&QuatAroundX, &GGVECTOR3(1, 0, 0), GGToRadian(ObjectAngleX(iObj)));
	GGQuaternionRotationAxis(&QuatAroundY, &GGVECTOR3(0, 1, 0), GGToRadian(ObjectAngleY(iObj)));
	GGQuaternionRotationAxis(&QuatAroundZ, &GGVECTOR3(0, 0, 1), GGToRadian(ObjectAngleZ(iObj)));
	quatRotationEvent = QuatAroundX * QuatAroundY * QuatAroundZ;
	float a = 2 * (quatRotationEvent.x * quatRotationEvent.z + quatRotationEvent.w * quatRotationEvent.y);
	float b = 1 - 2 * (quatRotationEvent.x * quatRotationEvent.x + quatRotationEvent.y * quatRotationEvent.y);
	float angRad = atan2(a, b);
	float angDeg = GGToDegree(angRad);
	if (angDeg < 0) angDeg += 360;
	float fOffsetX = GetObjectCollisionCenterX(iObj) * (ObjectScaleX(iObj) / 100.0f);
	float fOffsetZ = GetObjectCollisionCenterZ(iObj) * (ObjectScaleZ(iObj) / 100.0f);
	float x = t.entityelement[e].x + (fOffsetX * cos(angRad)) + (fOffsetZ * sin(angRad));
	float z = t.entityelement[e].z + (fOffsetZ * cos(angRad)) - (fOffsetX * sin(angRad));
	float sx = ObjectSizeX(iObj, 1) * 1.05f;
	float sz = ObjectSizeZ(iObj, 1) * 1.05f;

	if (iFlattenID == -1)
		t.entityelement[e].eleprof.iFlattenID = GGTerrain_AddVegetationArea(x, z, sx, sz, angDeg);
	else
		GGTerrain_UpdateVegetationArea(iFlattenID, x, z, sx, sz, angDeg);
	return true;
}

void entity_updateautoflatten (int e, int obj)
{
	if (entity_updatevegetationarea(e, obj)) return;
	int entid = t.entityelement[e].bankindex;
	if (entid > 0)
	{
		int iAutoFlattenMode = t.entityprofile[entid].autoflatten;

		if ((iAutoFlattenMode != 0 && !g.isGameBeingPlayed) && (!g_bEnableAutoFlattenSystem || !t.entityelement[e].eleprof.bAutoFlatten) )
		{
			if (t.entityelement[e].eleprof.iFlattenID != -1)
			{
				//PE: Disabled remove any flatten.
				GGTerrain_RemoveFlatArea(t.entityelement[e].eleprof.iFlattenID);
				t.entityelement[e].eleprof.iFlattenID = -1;
				//t.entityelement[e].eleprof.bAutoFlatten = false;
			}
		}
		else if (iAutoFlattenMode != 0 && !g.isGameBeingPlayed)
		{
			//PE: t.entityelement[e].obj is not set in creating process. entity_prepareobj()
			int iObj = t.entityelement[e].obj;
			if (iObj == 0 && obj > 0) iObj = obj; //PE: entity_prepareobj() now set obj for standalone.
			if (iObj > 0)
			{
				float x = t.entityelement[e].x + (GetObjectCollisionCenterX(iObj) * (ObjectScaleX(iObj) / 100.0f));
				float y = t.entityelement[e].y + (GetObjectCollisionCenterY(iObj) * (ObjectScaleY(iObj) / 100.0f)) - (ObjectSizeY(iObj) / 2, 1);
				float z = t.entityelement[e].z + (GetObjectCollisionCenterZ(iObj) * (ObjectScaleZ(iObj) / 100.0f));

				GGQUATERNION QuatAroundX, QuatAroundY, QuatAroundZ, quatRotationEvent;
				GGQuaternionRotationAxis(&QuatAroundX, &GGVECTOR3(1, 0, 0), GGToRadian(ObjectAngleX(iObj)));
				GGQuaternionRotationAxis(&QuatAroundY, &GGVECTOR3(0, 1, 0), GGToRadian(ObjectAngleY(iObj)));
				GGQuaternionRotationAxis(&QuatAroundZ, &GGVECTOR3(0, 0, 1), GGToRadian(ObjectAngleZ(iObj)));
				quatRotationEvent = QuatAroundX * QuatAroundY * QuatAroundZ;
				
				float a = 2 * (quatRotationEvent.x * quatRotationEvent.z + quatRotationEvent.w * quatRotationEvent.y);
				float b = 1 - 2 * (quatRotationEvent.x * quatRotationEvent.x + quatRotationEvent.y * quatRotationEvent.y);

				float angRad = atan2(a, b);
				float angDeg = GGToDegree(angRad);

				if (angDeg < 0) angDeg += 360;
				if (angDeg > 360) angDeg -= 360;

				float sx = ObjectSizeX(iObj, 1)* 1.05f + g_fFlattenMargin;
				float sz = ObjectSizeZ(iObj, 1) * 1.05f + g_fFlattenMargin;
				int iFlattenID = t.entityelement[e].eleprof.iFlattenID;

				if (iFlattenID != -1)
				{
					//PE: This is draining the CPU, constantly updating while selected.
					static float Lastx = -1, Lasty = -1, Lastz = -1, Lastang = -1, LastSizeX = 0.0f, LastSizeZ = 0.0f;
					static int iLastFlattenID = -1;
					if (y != Lasty || x != Lastx || z != Lastz || angDeg != Lastang || LastSizeX != sx || LastSizeZ != sz || iFlattenID != iLastFlattenID)
					{
						Lastx = x;
						Lasty = y;
						Lastz = z;
						Lastang = angDeg;
						LastSizeX = sx;
						LastSizeZ = sz;
						iLastFlattenID = iFlattenID;
						GGTerrain_UpdateFlatArea(iFlattenID, x, z, angDeg, entity_autoflattenpadsizex(iAutoFlattenMode, sx, sz), sz, t.entityelement[e].y);
					}
				}
			}
		}
	}
}


void entity_autoFlattenWhenAdded(int e, int obj)
{
	if (entity_updatevegetationarea(e, obj)) return;
	int entid = t.entityelement[e].bankindex;
	if (entid > 0)
	{
		int iAutoFlattenMode = t.entityprofile[entid].autoflatten;
		if (iAutoFlattenMode != 0)
		{
			int iObj = t.entityelement[e].obj;
			if (iObj == 0 && obj > 0) iObj = obj;

			//LB: never create autoflatten entity instance is a smart object instance
			if (t.entityelement[e].iIsSmarkobjectDummyObj == 1)
				return;

			// ZJ: when a map is saved, the level gets reloaded and iIsSmarkobjectDummyObj gets wiped out.
			// g_smartObjectDummyEntities stores all entities with iIsSmarkobjectDummyObj set before save
			extern std::vector<int> g_smartObjectDummyEntities;
			for (auto& dummyID : g_smartObjectDummyEntities)
			{
				if (dummyID == e)
				{
					return;
				}
			}
			// Also need to ensure that any levels that were saved with the infinite hole bug are restored
			// It's a hack, but not sure how else old levels can be fixed
			if (t.entityelement[e].x == 0 && t.entityelement[e].y == -500000 && t.entityelement[e].z == 0)
			{
				return;
			}

			float x = t.entityelement[e].x + (GetObjectCollisionCenterX(iObj) * (ObjectScaleX(iObj) / 100.0f));
			float y = t.entityelement[e].y + (GetObjectCollisionCenterY(iObj) * (ObjectScaleY(iObj) / 100.0f)) - (ObjectSizeY(iObj) / 2, 1);
			float z = t.entityelement[e].z + (GetObjectCollisionCenterZ(iObj) * (ObjectScaleZ(iObj) / 100.0f));

			GGQUATERNION QuatAroundX, QuatAroundY, QuatAroundZ, quatRotationEvent;
			GGQuaternionRotationAxis(&QuatAroundX, &GGVECTOR3(1, 0, 0), GGToRadian(ObjectAngleX(iObj)));
			GGQuaternionRotationAxis(&QuatAroundY, &GGVECTOR3(0, 1, 0), GGToRadian(ObjectAngleY(iObj)));
			GGQuaternionRotationAxis(&QuatAroundZ, &GGVECTOR3(0, 0, 1), GGToRadian(ObjectAngleZ(iObj)));
			quatRotationEvent = QuatAroundX * QuatAroundY * QuatAroundZ;
			float a = 2 * (quatRotationEvent.x * quatRotationEvent.z + quatRotationEvent.w * quatRotationEvent.y);
			float b = 1 - 2 * (quatRotationEvent.x * quatRotationEvent.x + quatRotationEvent.y * quatRotationEvent.y);
			float angRad = atan2(a, b);
			float angDeg = GGToDegree(angRad);
			if (angDeg < 0) angDeg += 360;
			if (angDeg > 360) angDeg -= 360;

			float sx = ObjectSizeX(iObj, 1) * 1.05f + g_fFlattenMargin;
			float sz = ObjectSizeZ(iObj, 1) * 1.05f + g_fFlattenMargin;

			if ((!g_bEnableAutoFlattenSystem || !t.entityelement[e].eleprof.bAutoFlatten))
			{
				if (t.entityelement[e].eleprof.iFlattenID != -1)
				{
					GGTerrain_RemoveFlatArea(t.entityelement[e].eleprof.iFlattenID);
					t.entityelement[e].eleprof.iFlattenID = -1;
				}
			}
			else if (t.entityelement[e].eleprof.iFlattenID == -1)
			{
				if (g_bEnableAutoFlattenSystem == true)
				{
					if (iAutoFlattenMode == 1)
					{
						t.entityelement[e].eleprof.iFlattenID = GGTerrain_AddFlatRect(x, z, sx, sz, angDeg, t.entityelement[e].y);
					}
					else
					{
						t.entityelement[e].eleprof.iFlattenID = GGTerrain_AddFlatCircle(x, z, entity_autoflattenpadsizex(iAutoFlattenMode, sx, sz), t.entityelement[e].y);
					}
				}
			}
			else
			{
				GGTerrain_UpdateFlatArea(t.entityelement[e].eleprof.iFlattenID, x, z, angDeg, entity_autoflattenpadsizex(iAutoFlattenMode, sx, sz), sz, t.entityelement[e].y);
			}
		}
		else
		{
			if (t.entityelement[e].eleprof.iFlattenID != -1)
			{
				GGTerrain_RemoveFlatArea(t.entityelement[e].eleprof.iFlattenID);
				t.entityelement[e].eleprof.iFlattenID = -1;
			}
		}
	}
}

bool ObjectIsEntity(void* pTestObject)
{
	sObject* pObject = (sObject*)pTestObject;
	for (int te = 1; te <= g.entityelementlist; te++)
	{
		if (t.entityelement[te].obj > 0 )
		{
			if (pObject->dwObjectNumber == t.entityelement[te].obj)
				return true;
		}
	}
	return false;
}

//These functions need a pMesh overwrite. so we later can save into map data, for per object material changes.
//pMesh overwrite will not work ,so everything is now moved to t.entityelement[ele_id].eleprof.bCustomWickedMaterialActive

//PE: if Element ID set , use t.entityelement[g_iWickedElementId].eleprof.WEMaterial for everything.
//PE: "Materials" to limit map.ele size we only allow changes to mesh 1 , and per object settings (checkbox+reflectence).

void WickedSetElementId(int ele_id)
{
	//Only set if bCustomWickedMaterialActive has been activated by user.
	if (ele_id > 0 && t.entityelement[ele_id].eleprof.bCustomWickedMaterialActive)
	{
		g_iWickedElementId = ele_id;
	}
	else
	{
		g_iWickedElementId = 0;
	}
}

void WickedSetUseEditorGrideleprof(bool bUse)
{
	g_bUseEditorGrideleprof = bUse;
}

bool IsWickedMaterialActive(void* pvMesh)
{
	sMesh* pMesh = (sMesh*)pvMesh;
	if(g_iWickedEntityId < 0) return false;
	if( g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive )
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive;
	return t.entityprofile[g_iWickedEntityId].WEMaterial.MaterialActive;
}

void WickedSetEntityId(int ent_id)
{
	g_iWickedEntityId = ent_id;
}
int WickedGetEntityId(void)
{
	return g_iWickedEntityId;
}

float WickedGetTreeAlphaRef(void)
{
	float TreeAlphaRef = 1.0f;
	if (g_iWickedEntityId >= 0) { //PE: auto thumbs is now using 0.
		if (strcmp(Lower(t.entityprofile[g_iWickedEntityId].effect_s.Get()), "effectbank\\reloaded\\apbr_tree.fx") == 0)
		{
			TreeAlphaRef = 0.41f;
		}
		if (strcmp(Lower(t.entityprofile[g_iWickedEntityId].effect_s.Get()), "effectbank\\reloaded\\apbr_treea.fx") == 0)
		{
			TreeAlphaRef = 0.41f;
		}
	}
	return(TreeAlphaRef);
}

void WickedSetMeshNumber(int iMNumber)
{
	//PE: Got a crash when reaching 100 as index is only 0-99
	if (g_iWickedEntityId < 0 && iMNumber >= MAXMESHMATERIALS - 1)
		g_iWickedMeshNumber = 0;
	else
	{
		if(iMNumber >= MAXMESHMATERIALS - 1)
			g_iWickedMeshNumber = 0;
		else
			g_iWickedMeshNumber = iMNumber;
	}
}

cStr ReturnEmpty = "";

cStr WickedGetBaseColorName( void )
{
	if (g_iWickedEntityId < 0) return ReturnEmpty;
	if ( g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber];
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.baseColorMapName[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS ) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.baseColorMapName[g_iWickedMeshNumber];
	}
	return ReturnEmpty;
}

cStr WickedGetNormalName(void)
{
	if (g_iWickedEntityId < 0) return ReturnEmpty;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.normalMapName[g_iWickedMeshNumber];
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.normalMapName[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.normalMapName[g_iWickedMeshNumber].Len() > 0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.normalMapName[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.normalMapName[g_iWickedMeshNumber];
	}
	return ReturnEmpty;
}

cStr WickedGetSurfaceName(void)
{
	if (g_iWickedEntityId < 0) return ReturnEmpty;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.surfaceMapName[g_iWickedMeshNumber];
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.surfaceMapName[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.surfaceMapName[g_iWickedMeshNumber].Len() > 0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.surfaceMapName[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.surfaceMapName[g_iWickedMeshNumber];
	}
	return ReturnEmpty;
}

cStr WickedGetDisplacementName(void)
{
	if (g_iWickedEntityId < 0) return ReturnEmpty;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.displacementMapName[g_iWickedMeshNumber];
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.displacementMapName[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.displacementMapName[g_iWickedMeshNumber].Len() > 0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.displacementMapName[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.displacementMapName[g_iWickedMeshNumber];
	}
	return ReturnEmpty;
}

cStr WickedGetEmissiveName(void)
{
	if (g_iWickedEntityId < 0) return ReturnEmpty;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.emissiveMapName[g_iWickedMeshNumber];
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.emissiveMapName[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.emissiveMapName[g_iWickedMeshNumber].Len() > 0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.emissiveMapName[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.emissiveMapName[g_iWickedMeshNumber];
	}
	return ReturnEmpty;
}

cStr WickedGetOcclusionName(void)
{
	#ifdef DISABLEOCCLUSIONMAP
	if (g_iWickedEntityId < 0) return ReturnEmpty;
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0)
	{
		LPSTR pBaseTextureFilename = t.entityprofile[g_iWickedEntityId].WEMaterial.baseColorMapName[g_iWickedMeshNumber].Get();
		LPSTR pBaseColorFileExt = "_color.dds";
		if (strnicmp(pBaseTextureFilename + strlen(pBaseTextureFilename) - strlen(pBaseColorFileExt), pBaseColorFileExt, strlen(pBaseColorFileExt)-3) != NULL)
		{
			pBaseColorFileExt = "_basecolor.dds";
			if (strnicmp(pBaseTextureFilename + strlen(pBaseTextureFilename) - strlen(pBaseColorFileExt), pBaseColorFileExt, strlen(pBaseColorFileExt) - 3) != NULL)
			{
				// no matches found (ignoring file extension (dds,png))
				pBaseColorFileExt = "";
			}
		}
		if(strlen(pBaseColorFileExt)>0)
		{
			char pFullOcclusionFilename[MAX_PATH];
			strcpy(pFullOcclusionFilename, pBaseTextureFilename);
			pFullOcclusionFilename[strlen(pBaseTextureFilename) - strlen(pBaseColorFileExt)] = 0;
			char pFinalFilename[MAX_PATH];
			strcpy(pFinalFilename, pFullOcclusionFilename);
			strcat(pFinalFilename, "_height.dds");
			extern std::string g_pWickedTexturePath;
			cstr ensureCorrectFormatExt = cstr((LPSTR)g_pWickedTexturePath.c_str()) + pFinalFilename;
			if (FileExist((LPSTR)ensureCorrectFormatExt.Get()) == 0)
			{
				strcpy(pFinalFilename, pFullOcclusionFilename);
				strcat(pFinalFilename, "_height.png");
				ensureCorrectFormatExt = cstr((LPSTR)g_pWickedTexturePath.c_str()) + pFinalFilename;
			}
			cStr fullOcclusionFilename_s = ensureCorrectFormatExt;
			return fullOcclusionFilename_s;
		}
	}
	return ReturnEmpty;
	#else
	if (g_iWickedEntityId < 0) return ReturnEmpty;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.occlusionMapName[g_iWickedMeshNumber];
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.occlusionMapName[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.occlusionMapName[g_iWickedMeshNumber].Len() > 0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.occlusionMapName[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.occlusionMapName[g_iWickedMeshNumber];
	}
	return ReturnEmpty;
	#endif
}

float WickedGetNormalStrength(void)
{
	if (g_iWickedEntityId < 0) return 1.0f;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			if(t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fNormal[g_iWickedMeshNumber] >= 0.0f)
				return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fNormal[g_iWickedMeshNumber];
			else
				return(1.0f);
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fNormal[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.fNormal[g_iWickedMeshNumber] >= 0.0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fNormal[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS && t.entityprofile[g_iWickedEntityId].WEMaterial.fNormal[g_iWickedMeshNumber] >= 0.0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fNormal[g_iWickedMeshNumber];
	}
	return 1.0f;
}

float WickedGetRoughnessStrength(void)
{
	if (g_iWickedEntityId < 0) return 0.2f;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fRoughness[g_iWickedMeshNumber] >= 0.0f)
				return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fRoughness[g_iWickedMeshNumber];
			else
				return(0.2f);
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fRoughness[g_iWickedMeshNumber];
	}
	// GGMAX bug fix (2026-08-04): FPE-sourced roughness — treat an explicit 0.00 as UNSET,
	// not "perfect mirror". The DX11 Model Importer wrote roughnessStrength as the RAW DCC
	// material roughness (DX11 M-Importer.cpp:7541/7817), where 0.00 just means the artist
	// never authored the channel — and the DX11 renderer never consumed the field at all.
	// 547 shipped FPEs carry roughnessStrength=0.00; honoring it literally made them
	// glass-smooth mirrors (TESTPRO1 breakables barrels: basecolor washed out by
	// grazing-angle probe reflections — "incomplete reflection" bug). This value is consumed
	// ONLY when a surface map loaded (wickedcalls_part1.cpp:305-309) and it MULTIPLIES the
	// map's roughness channel, so "unset" must be 1.0 — let the authored map drive roughness,
	// exactly what the 5,580 healthy roughnessStrength=1.00 assets do. The editor material
	// panel path above still honors an explicit 0 (deliberate user choice); an FPE that
	// truly wants a mirror can say roughnessStrength=0.01.
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.fRoughness[g_iWickedMeshNumber] > 0.0f)
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fRoughness[g_iWickedMeshNumber];
	}
	return 1.0f;
}
float WickedGetMetallnessStrength(void)
{
	if (g_iWickedEntityId < 0) return 0.0f;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fMetallness[g_iWickedMeshNumber] >= 0.0f)
				return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fMetallness[g_iWickedMeshNumber];
			else
				return(0.0f);
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fMetallness[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.fMetallness[g_iWickedMeshNumber] >= 0.0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fMetallness[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS && t.entityprofile[g_iWickedEntityId].WEMaterial.fMetallness[g_iWickedMeshNumber] >= 0.0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fMetallness[g_iWickedMeshNumber];
	}
	return 0.0f;
}

float WickedGetEmissiveStrength(void)
{
	if (g_iWickedEntityId < 0) return 0.0f;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fEmissive[g_iWickedMeshNumber] >= 0.0f)
				return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fEmissive[g_iWickedMeshNumber];
			else
				return(0.0f);
		}
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fEmissive[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.fEmissive[g_iWickedMeshNumber] >= 0.0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fEmissive[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS && t.entityprofile[g_iWickedEntityId].WEMaterial.fEmissive[g_iWickedMeshNumber] >= 0.0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fEmissive[g_iWickedMeshNumber];
	}
	return 0.0f;
}

float WickedGetAlphaRef(void)
{
	if (g_iWickedEntityId < 0) return 1.0f;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.baseColorMapName[g_iWickedMeshNumber].Len() > 0) 
		{
			if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fAlphaRef[g_iWickedMeshNumber] < 0)
				return(1.0f);
			return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fAlphaRef[g_iWickedMeshNumber];
		}
		if (t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fAlphaRef[g_iWickedMeshNumber] < 0)
			return(1.0f);
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fAlphaRef[g_iWickedMeshNumber];
	}
	if (t.entityprofile[g_iWickedEntityId].WEMaterial.fAlphaRef[g_iWickedMeshNumber] >= 0.0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fAlphaRef[g_iWickedMeshNumber];
	}
	if (g_iWickedMeshNumber >= 0 && g_iWickedMeshNumber < MAXMESHMATERIALS && t.entityprofile[g_iWickedEntityId].WEMaterial.fAlphaRef[g_iWickedMeshNumber] >= 0.0) 
	{
		return t.entityprofile[g_iWickedEntityId].WEMaterial.fAlphaRef[g_iWickedMeshNumber];
	}
	return 1.0f;
}

bool WickedDoubleSided(void)
{
	if (g_iWickedEntityId < 0) return false;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.bDoubleSided[g_iWickedMeshNumber];
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.bDoubleSided[g_iWickedMeshNumber];
}

int WickedCustomShaderID(void)
{
	if (g_iWickedEntityId < 0) return -1;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.customShaderID;
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.customShaderID;
}
float WickedCustomShaderParam1(void)
{
	if (g_iWickedEntityId < 0) return 0;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.customShaderParam1;
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.customShaderParam1;
}
float WickedCustomShaderParam2(void)
{
	if (g_iWickedEntityId < 0) return 0;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.customShaderParam2;
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.customShaderParam2;
}
float WickedCustomShaderParam3(void)
{
	if (g_iWickedEntityId < 0) return 0;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.customShaderParam3;
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.customShaderParam3;
}
float WickedCustomShaderParam4(void)
{
	if (g_iWickedEntityId < 0) return 0;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.customShaderParam4;
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.customShaderParam4;
}
float WickedCustomShaderParam5(void)
{
	if (g_iWickedEntityId < 0) return 0;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.customShaderParam5;
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.customShaderParam5;
}
float WickedCustomShaderParam6(void)
{
	if (g_iWickedEntityId < 0) return 0;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.customShaderParam6;
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.customShaderParam6;
}
float WickedCustomShaderParam7(void)
{
	if (g_iWickedEntityId < 0) return 0;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.customShaderParam7;
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.customShaderParam7;
}

float WickedRenderOrderBias(void)
{
	if (g_iWickedEntityId < 0) return false;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fRenderOrderBias[g_iWickedMeshNumber];
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.fRenderOrderBias[g_iWickedMeshNumber];
}

bool WickedGetTransparent(void)
{
	bool bIsTransparent = false;
	if (g_iWickedEntityId < 0) return false;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
		bIsTransparent = t.entityelement[g_iWickedElementId].eleprof.WEMaterial.bTransparency[g_iWickedMeshNumber];
	else
		bIsTransparent = t.entityprofile[g_iWickedEntityId].WEMaterial.bTransparency[g_iWickedMeshNumber];

	return(bIsTransparent);
}

bool WickedGetCastShadows(void)
{
	if (g_iWickedEntityId < 0) return false;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.bCastShadows[g_iWickedMeshNumber];
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.bCastShadows[g_iWickedMeshNumber];
}

bool WickedPlanerReflection(void)
{
	if (g_iWickedEntityId < 0) return false;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.bPlanerReflection[g_iWickedMeshNumber];
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.bPlanerReflection[g_iWickedMeshNumber];
}

float WickedGetReflectance(void)
{
	if (g_iWickedEntityId < 0)
		return 0.04f;// 0.002f;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.fReflectance[g_iWickedMeshNumber];
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.fReflectance[g_iWickedMeshNumber];
}

DWORD WickedGetEmmisiveColor(void)
{
	if (g_iWickedEntityId < 0) return -1;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.dwEmmisiveColor[g_iWickedMeshNumber];
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.dwEmmisiveColor[g_iWickedMeshNumber];
}

DWORD WickedGetBaseColor(void)
{
	if (g_iWickedEntityId < 0) return -1;
	if (g_iWickedElementId > 0 && t.entityelement[g_iWickedElementId].eleprof.WEMaterial.MaterialActive)
	{
		return t.entityelement[g_iWickedElementId].eleprof.WEMaterial.dwBaseColor[g_iWickedMeshNumber];
	}
	return t.entityprofile[g_iWickedEntityId].WEMaterial.dwBaseColor[g_iWickedMeshNumber];
}

void Wicked_Highlight_ClearAllObjects(void)
{
	for (int e = 1; e <= g.entityelementlist; e++)
	{
		int obj = t.entityelement[e].obj;
		if (obj > 0)
		{
			sObject* pObject = GetObjectData(obj);
			if (pObject)
			{
				WickedCall_SetObjectHighlightBlue(pObject, false);
			}
		}
	}
}

void Wicked_Highlight_AllLogicObjects(void)
{
	for (int e = 1; e <= g.entityelementlist; e++)
	{
		// only highlight the logic objects in blue
		if (t.entityelement[e].staticflag == 0)
		{
			int obj = t.entityelement[e].obj;
			if (obj > 0)
			{
				sObject* pObject = GetObjectData(obj);
				if (pObject)
				{
					WickedCall_SetObjectHighlightBlue(pObject, true);
					g_ObjectHighlightList.push_back(obj);
				}
			}
		}
	}
}

void Wicked_Highlight_Rubberband(void)
{
	if (t.grideditselect == 5 && g.entityrubberbandlist.size() > 0)
	{
		for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
		{
			int e = g.entityrubberbandlist[i].e;
			if (e >= t.entityelement.size())
			{
				g.entityrubberbandlist.clear();
				return;
			}
			int tobj = t.entityelement[e].obj;
			if (tobj > 0)
			{
				sObject* pObject = GetObjectData(tobj);
				if (pObject) {
					void WickedCall_DrawObjctBox(sObject* pObject, XMFLOAT4 color, bool bThickLine = false, bool ForceBox = false);
					WickedCall_DrawObjctBox(pObject, XMFLOAT4(0.75f, 0.75f, 0.75f, 0.75f),false,false);
				}
			}
		}
	}
}

std::vector<sRubberBandType> entityselectionlist;
void Wicked_Highlight_Selection(void)
{
	if (t.grideditselect == 5 && entityselectionlist.size() > 0)
	{
		for (int i = 0; i < (int)entityselectionlist.size(); i++)
		{
			int e = entityselectionlist[i].e;
			int tobj = t.entityelement[e].obj;
			if (tobj > 0)
			{
				sObject* pObject = GetObjectData(tobj);
				if (pObject) {
					void WickedCall_DrawObjctBox(sObject* pObject, XMFLOAT4 color, bool bThickLine = false, bool ForceBox = false);
					WickedCall_DrawObjctBox(pObject, XMFLOAT4(1.0f, 0.75f, 0.0f, 0.85f), true,true);
				}
			}
		}
	}
}

extern std::vector<sRubberBandType> vEntityLockedList;
void Wicked_Highlight_LockedList(void)
{
	if (t.grideditselect == 5 && vEntityLockedList.size() > 0)
	{
		for (int i = 0; i < (int)vEntityLockedList.size(); i++)
		{
			int e = vEntityLockedList[i].e;
			if (e < 0 || e >= t.entityelement.size()) continue;

			#ifdef ALLOWSELECTINGLOCKEDOBJECTS
			//PE: Only diplay red on selected items.
			bool bDisplayRed = false;
			if (e == t.widget.pickedEntityIndex)
				bDisplayRed = true;
			if (!bDisplayRed)
			{
				//PE: Also need rubberband items. (if within a selected group).
				if (g.entityrubberbandlist.size() > 0)
				{
					for (int ir = 0; ir < (int)g.entityrubberbandlist.size(); ir++)
					{
						if (e == g.entityrubberbandlist[ir].e)
						{
							bDisplayRed = true;
							break;
						}
					}
				}
			}
				
			if(bDisplayRed)
			{
			#endif

				int tobj = t.entityelement[e].obj;
				if (tobj > 0)
				{
					sObject* pObject = GetObjectData(tobj);
					if (pObject) {
						void WickedCall_DrawObjctBox(sObject* pObject, XMFLOAT4 color, bool bThickLine = false, bool ForceBox = false);
						WickedCall_DrawObjctBox(pObject, XMFLOAT4(1.0f, 0.0f, 0.0f, 0.85f), false, false);
					}
				}
			#ifdef ALLOWSELECTINGLOCKEDOBJECTS
			}
			#endif
		}
	}
}

void Wicked_Ignore_Frame_Mesh(int obj)
{
	if (obj <= 0 || obj > MAXIMUMVALUE) return;
	sObject* pObject = g_ObjectList[obj];
	if (!pObject) return;

	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i])
		{
			pObject->ppFrameList[i]->bIgnoreMesh = false;
		}
	}

	extern bool bDisableLODLoad;
	if (bDisableLODLoad) return;

	int bestlod = -1;
	bool bHasLOD = false;

	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		sFrame* pFrame = pObject->ppFrameList[i];
		if (pFrame)
		{
			LPSTR pName = pFrame->szName;
			if (pName && strlen(pName) > 0)
			{
				char* r5 = nullptr;
				cstr lname = Lower(pName);
				if (strlen(lname.Get()) >= 5)
				{
					r5 = lname.Get() + strlen(lname.Get()) - 5;
					if (r5)
					{
						if ((stricmp(r5, "lod_1") == 0 || stricmp(r5, "_lod1") == 0))
							bHasLOD = true;
						if ((stricmp(r5, "lod_2") == 0 || stricmp(r5, "_lod2") == 0))
							bHasLOD = true;

						if ((stricmp(r5, "lod_1") == 0 || stricmp(r5, "_lod1") == 0) && (bestlod == -1 || bestlod > 1))
						{
							bestlod = 1;
						}
						else
						{
							if ((stricmp(r5, "lod_2") == 0 || stricmp(r5, "_lod2") == 0) && (bestlod == -1))
							{
								bestlod = 2;
							}
							else
							{
								bestlod = 0;
							}
						}

					}
				}

				if (stricmp(lname.Get(), "collision_mesh") == 0)
				{
					pFrame->bIgnoreMesh = true;
				}
				//PE: UCX unreal collision mesh must include LOD
				if (strnicmp(lname.Get(), "UCX_", 4) == 0 && r5 && stricmp(r5, "_lod0") == 0)
				{
					pFrame->bIgnoreMesh = true;
				}
			}
		}
	}

	if (bHasLOD)
	{
		for (int i = 0; i < pObject->iFrameCount; i++)
		{
			sFrame* pFrame = pObject->ppFrameList[i];
			if (pFrame)
			{
				LPSTR pName = pFrame->szName;
				if (pName && strlen(pName) > 0)
				{
					char* r5 = nullptr;
					cstr lname = Lower(pName);
					if (strlen(lname.Get()) >= 5)
					{
						r5 = lname.Get() + strlen(lname.Get()) - 5;
						if (bestlod == 0 && (stricmp(r5, "lod_1") == 0 || stricmp(r5, "lod_2") == 0 || stricmp(r5, "lod_3") == 0))
						{
							pFrame->bIgnoreMesh = true;
						}
						if (bestlod == 0 && (stricmp(r5, "_lod1") == 0 || stricmp(r5, "_lod2") == 0 || stricmp(r5, "_lod3") == 0))
						{
							pFrame->bIgnoreMesh = true;
						}
						if (bestlod == 1 && (stricmp(r5, "lod_2") == 0 || stricmp(r5, "_lod2") == 0))
						{
							pFrame->bIgnoreMesh = true;
						}
						if (bestlod == 2 && (stricmp(r5, "lod_3") == 0 || stricmp(r5, "_lod3") == 0))
						{
							pFrame->bIgnoreMesh = true;
						}
					}
				}
			}
		}
	}
}

void Wicked_Hide_Lower_Lod_Meshes(int obj)
{
	PerformCheckListForLimbs(obj);
	int bestlod = -1;
	bool bHasLOD = false;
	for (t.c = ChecklistQuantity(); t.c >= 1; t.c += -1)
	{
		t.tname_s = Lower(ChecklistString(t.c));
		LPSTR pRightFive = "";
		if (strlen(t.tname_s.Get()) >= 5) pRightFive = t.tname_s.Get() + strlen(t.tname_s.Get()) - 5;

		if ((stricmp(pRightFive, "lod_1") == 0 || stricmp(pRightFive, "_lod1") == 0) )
			bHasLOD = true;
		if ((stricmp(pRightFive, "lod_2") == 0 || stricmp(pRightFive, "_lod2") == 0))
			bHasLOD = true;

		if ((stricmp(pRightFive, "lod_1") == 0 || stricmp(pRightFive, "_lod1") == 0) && (bestlod == -1 || bestlod > 1))
		{
			bestlod = 1;
		}
		else
		{
			if ((stricmp(pRightFive, "lod_2") == 0 || stricmp(pRightFive, "_lod2") == 0) && (bestlod == -1))
			{
				bestlod = 2;
			}
			else
			{
				// base (highest) LOD does not need the lod0 or _lod postfix
				bestlod = 0;
			}
		}

		// also hide ANY collision_mesh frame as this should never be visible
		if (stricmp(t.tname_s.Get(), "collision_mesh") == 0)
		{
			HideLimb(obj, t.c - 1);
		}
	}
	if (bHasLOD)
	{
		for (t.c = ChecklistQuantity(); t.c >= 1; t.c += -1)
		{
			t.tname_s = Lower(ChecklistString(t.c));
			LPSTR pRightFive = "";
			if (strlen(t.tname_s.Get()) >= 5) pRightFive = t.tname_s.Get() + strlen(t.tname_s.Get()) - 5;
			if (bestlod == 0 && (stricmp(pRightFive, "lod_1") == 0 || stricmp(pRightFive, "lod_2") == 0 || stricmp(pRightFive, "lod_3") == 0)) 
			{
				HideLimb(obj, t.c - 1);
			}
			if (bestlod == 0 && (stricmp(pRightFive, "_lod1") == 0 || stricmp(pRightFive, "_lod2") == 0 || stricmp(pRightFive, "_lod3") == 0)) 
			{
				HideLimb(obj, t.c - 1);
			}
			if (bestlod == 1 && (stricmp(pRightFive, "lod_2") == 0 || stricmp(pRightFive, "_lod2") == 0)) 
			{
				HideLimb(obj, t.c - 1);
			}
			if (bestlod == 2 && (stricmp(pRightFive, "lod_3") == 0 || stricmp(pRightFive, "_lod3") == 0)) 
			{
				HideLimb(obj, t.c - 1);
			}
		}
	}
}


int GetLodLevels(int obj)
{
	PerformCheckListForLimbs(obj);
	int iLodLevels = 0;
	int bestlod = -1;
	bool bHasLOD = false;
	for (t.c = ChecklistQuantity(); t.c >= 1; t.c += -1)
	{
		t.tname_s = Lower(ChecklistString(t.c));
		LPSTR pRightFive = "";
		if (strlen(t.tname_s.Get()) >= 5) pRightFive = t.tname_s.Get() + strlen(t.tname_s.Get()) - 5;
		if ((stricmp(pRightFive, "lod_1") == 0 || stricmp(pRightFive, "_lod1") == 0))
			iLodLevels++;
		else if ((stricmp(pRightFive, "lod_2") == 0 || stricmp(pRightFive, "_lod2") == 0))
			iLodLevels++;
		else if ((stricmp(pRightFive, "lod_3") == 0 || stricmp(pRightFive, "_lod3") == 0))
			iLodLevels++;

	}
	return(iLodLevels);
}

void entity_updatequatfromeuler (int e)
{
	// update entity quat as the preferred source rotation
	GGQUATERNION QuatAroundX, QuatAroundY, QuatAroundZ;
	GGQuaternionRotationAxis(&QuatAroundX, &GGVECTOR3(1, 0, 0), GGToRadian(t.entityelement[e].rx));
	GGQuaternionRotationAxis(&QuatAroundY, &GGVECTOR3(0, 1, 0), GGToRadian(t.entityelement[e].ry));
	GGQuaternionRotationAxis(&QuatAroundZ, &GGVECTOR3(0, 0, 1), GGToRadian(t.entityelement[e].rz));
	GGQUATERNION quatNewOrientation = QuatAroundX * QuatAroundY * QuatAroundZ;
	t.entityelement[e].quatmode = 1;
	t.entityelement[e].quatx = quatNewOrientation.x;
	t.entityelement[e].quaty = quatNewOrientation.y;
	t.entityelement[e].quatz = quatNewOrientation.z;
	t.entityelement[e].quatw = quatNewOrientation.w;
}

void entity_updatequat (int e, float quatx, float quaty, float quatz, float quatw)
{
	t.entityelement[e].quatmode = 1;
	t.entityelement[e].quatx = quatx;
	t.entityelement[e].quaty = quaty;
	t.entityelement[e].quatz = quatz;
	t.entityelement[e].quatw = quatw;
}

void entity_calculateeuleryfromquat (int e)
{
	if (t.entityelement[e].quatmode != 0 ) // seems some corruption can given this value a non-zero but still have quat rot data
	{
		// only do if have a good quat
		GGQUATERNION Quat = GGQUATERNION(t.entityelement[e].quatx, t.entityelement[e].quaty, t.entityelement[e].quatz, t.entityelement[e].quatw);
		GGMATRIX matQuatRot;
		GGMatrixRotationQuaternion(&matQuatRot, &Quat);
		GGVECTOR3 positionalOffset = GGVECTOR3(0, 0, 1);
		GGVec3TransformCoord(&positionalOffset, &positionalOffset, &matQuatRot);
		float fRealWorldYAngle = Atan2(positionalOffset.x, positionalOffset.z);
		t.entityelement[e].rx = 0;
		t.entityelement[e].ry = fRealWorldYAngle;
		t.entityelement[e].rz = 0;

		// ensure generated euler is exactly matching quat (and sets quatmode to 1 in case of older level corruption data)
		entity_updatequatfromeuler(e);
	}
	else
	{
		// otherwise we already have the euler values
	}
}

#ifndef GGMAXEPIC
#include "M-Workshop.h"
extern std::vector<sWorkshopItem> g_workshopItemsList;
extern std::vector<sWorkshopSteamUserName> g_workshopSteamUserNames;
extern std::vector<PublishedFileId_t> g_workshopTrustedItems;
bool workshop_verifyandorreplacescript(int e, int entid)
{
	return false;
}
#else
bool workshop_verifyandorreplacescript(int e, int entid)
{
	// does nothing in EPIC mode
	return false;
}
#endif
