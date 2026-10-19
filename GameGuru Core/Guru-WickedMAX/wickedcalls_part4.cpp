#ifdef WICKEDPARTICLESYSTEM // continued from wickedcalls_part3.cpp
//iAction = 1 Burst all. 2 = Pause. - 3 = Resume. - 4 = Restart - 5 - visible - 6 = not visible. - 7 = pause emit - 8 = resume emit
void WickedCall_PerformEmitterAction(int iAction, uint32_t emitter_root)
{

	Scene& scene = wiScene::GetScene();

	//PE: Scan emitters.
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
		if (hier)
		{
			if (hier->parentID == emitter_root)
			{
				wiEmittedParticle* ec = scene.emitters.GetComponent(emitter);
				switch (iAction)
				{
					case 1:
					{
						ec->Burst(0);
						break;
					}
					case 2:
					{
						ec->SetPaused(true);
						break;
					}
					case 3:
					{
						ec->SetPaused(false);
						break;
					}
					case 4:
					{
						ec->Restart();
						break;
					}
					// GGMAX 2.00: actions 5-8 restored. They were commented out during the
					// DX12 API migration because the modern emitter had no equivalents;
					// the members are back (see wiEmittedParticle.h). Action 6 is the
					// most-called of all of them - without it editor previews could never
					// be hidden, and weapon trails (action 7) never stopped emitting.
					case 5:
					{
						ec->SetVisible(true);
						break;
					}
					case 6:
					{
						ec->SetVisible(false);
						break;
					}
					case 7:
					{
						ec->SetEmitPaused(true);
						break;
					}
					case 8:
					{
						ec->SetEmitPaused(false);
						break;
					}
				}
			}
		}
	}
}

// SetGamePaused: hold every running emitter still (its particles are still drawn), then release only those it held
void WickedCall_PauseEmitters(bool bPause)
{
	static std::vector<Entity> heldEmitters;
	Scene& scene = wiScene::GetScene();
	if (bPause)
	{
		heldEmitters.clear();
		for (size_t i = 0; i < scene.emitters.GetCount(); i++)
		{
			wiEmittedParticle& ec = scene.emitters[i];
			if (!ec.IsPaused())
			{
				ec.SetPaused(true);
				heldEmitters.push_back(scene.emitters.GetEntity(i));
			}
		}
	}
	else
	{
		for (size_t i = 0; i < heldEmitters.size(); i++)
		{
			wiEmittedParticle* ec = scene.emitters.GetComponent(heldEmitters[i]);
			if (ec) ec->SetPaused(false);
		}
		heldEmitters.clear();
	}
}

bool WickedCall_ParticleEffectPositionRotation(uint32_t root, float fX, float fY, float fZ, float fXa, float fYa, float fZa)
{
	Scene& scene = wiScene::GetScene();
	TransformComponent* root_tranform = scene.transforms.GetComponent(root);
	if (root_tranform)
	{
		root_tranform->ClearTransform();

		float rotationRadiansX = fXa * (XM_PI / 180.0f);
		float rotationRadiansY = fYa * (XM_PI / 180.0f);
		float rotationRadiansZ = fZa * (XM_PI / 180.0f);
		XMFLOAT3 rot = { rotationRadiansX ,rotationRadiansY ,rotationRadiansZ }; //PE: 0 - XM_2PI
		root_tranform->RotateRollPitchYaw(rot);

		root_tranform->Translate(XMFLOAT3(fX, fY, fZ));
		root_tranform->UpdateTransform();
		return true;
	}
	return false;
}

bool WickedCall_ParticleEffectPosition(uint32_t root, float fX, float fY, float fZ)
{
	Scene& scene = wiScene::GetScene();
	TransformComponent* root_tranform = scene.transforms.GetComponent(root);
	if (root_tranform)
	{
		root_tranform->ClearTransform();
		root_tranform->Translate(XMFLOAT3(fX, fY, fZ));
		root_tranform->UpdateTransform();
		return true;
	}
	return false;
}

//#define WPEDebug
void WickedCall_UpdateEmitters(void)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	//PE: Scan emitters.
	std::vector< uint32_t> parent_used;
	parent_used.clear();
	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		wiEmittedParticle* ec = scene.emitters.GetComponent(emitter);

#ifdef WPEDebug
		if (ec && ec->IsVolumeEnabled())
		{
			XMFLOAT4X4 hoverBox;
			wiScene::TransformComponent* pTransform = wiScene::GetScene().transforms.GetComponent(emitter);
			if (pTransform)
			{
				if (1)
				{
					AABB aabb;
					XMFLOAT3 pos = pTransform->GetPosition();

					XMFLOAT3 sca = pTransform->GetScale();
					aabb._min = XMFLOAT3(pos.x - sca.x, pos.y, pos.z - sca.z);
					aabb._max = XMFLOAT3(pos.x + sca.x, pos.y + sca.y, pos.z + sca.z);

					XMStoreFloat4x4(&hoverBox, aabb.getAsBoxMatrix()); // *pTransform->GetLocalMatrix());
					XMVECTOR S, R, T;
					XMMatrixDecompose(&S, &R, &T, XMLoadFloat4x4(&hoverBox));

					//XMVECTOR R_local = XMLoadFloat4(&root_tranform->rotation_local);
					XMVECTOR R_local = XMLoadFloat4(&pTransform->rotation_local);
					XMStoreFloat4x4(&hoverBox,
						XMMatrixScalingFromVector(S) *
						XMMatrixRotationQuaternion(R_local) *
						XMMatrixTranslationFromVector(T));

					wiRenderer::DrawBox(hoverBox, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));
				}
			}
		}
#endif

		//PE: If bFollowCamera , find InDoor , OutDoor , UnderWater.
		//PE: bFindFloor ONLY if ec->bFollowCamera
		// GGMAX 2.00: restored. This is what makes weather volumes (rain, downpour,
		// heavy-rain3, dust) track the player instead of sitting where they were placed.
		if (ec && (ec->bFindFloor || ec->bFollowCamera))
		{
			HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
			if (hier)
			{
				// GGMAX 3.111: never the Particle Library's own preview effect. It is parked in front of the
				// library's preview camera, 39000 units up, and dragging it to the EDITOR camera every frame
				// guaranteed a follow-camera effect could not appear in the hover preview or the thumbnail.
				// (Measured: birds, downpour and heavy-rain3 still draw nothing visible at preview scale -
				// their thumbnails keep the PARTICLE icon, see GGThumbGen_Hold.)
				extern uint32_t g_LibraryPreviewWPERoot;
				if (hier->parentID != wiECS::INVALID_ENTITY && !(g_LibraryPreviewWPERoot != 0 && hier->parentID == g_LibraryPreviewWPERoot))
				{
					bool bAlreadySet = false;
					// GGMAX 2.00: was 'a > parent_used.size()' - the loop never ran, so the
					// same root could be repositioned once per emitter under it. Bug is
					// present in the DX11 original too; fixed here now the block is live.
					for (int a = 0; a < (int)parent_used.size(); a++)
					{
						if (parent_used[a] == hier->parentID)
						{
							bAlreadySet = true;
							break;
						}
					}
					if (!bAlreadySet)
					{
						parent_used.push_back(hier->parentID);
						TransformComponent* root_tranform = scene.transforms.GetComponent(hier->parentID);
						if (root_tranform)
						{

							if (ec->bFollowCamera)
							{
								float fX, fY, fZ;
								fX = CameraPositionX();
								//PE: PlayerHeight might have to be removed in GGM
								fY = CameraPositionY(); // -PlayerHeight;
								fZ = CameraPositionZ();
								root_tranform->ClearTransform();
								root_tranform->Translate(XMFLOAT3(fX, fY, fZ));
								root_tranform->UpdateTransform();
							}
							if (ec->bFindFloor && ec->bFollowCamera)
							{
								float fX = root_tranform->GetPosition().x;
								float fZ = root_tranform->GetPosition().z;
								float height = BT_GetGroundHeight(t.terrain.TerrainID, CameraPositionX(), CameraPositionZ());
								root_tranform->ClearTransform();
								root_tranform->Translate(XMFLOAT3(fX, height, fZ));
								root_tranform->UpdateTransform();
							}
						}
					}
				}
			}
		}
	}
}

uint32_t WickedCall_CreateEmitter(std::string& name, float posX, float posY, float posZ, uint32_t proot)
{
	XMFLOAT3 position = { posX , posY, posZ };

	//wiScene::Scene& scene = wiScene::GetScene();
	Scene scene;
	XMMATRIX& transformMatrix = XMMatrixIdentity();

	//PE: Create emitter.
	Entity entity = CreateEntity();

	scene.names.Create(entity) = name;
	scene.emitters.Create(entity).count = 10;

	wiEmittedParticle* ec;
	ec = scene.emitters.GetComponent(entity);
	ec->count = 40;
	ec->life = 2.5f;
	ec->size = 2;
	ec->gravity = XMFLOAT3(0, 9, 0);

	TransformComponent& transform = scene.transforms.Create(entity);
	transform.ClearTransform();
	transform.Translate(position);
	transform.Scale(XMFLOAT3(3, 1, 3));
	transform.UpdateTransform();

	scene.materials.Create(entity).userBlendMode = BLENDMODE_ADDITIVE; // BLENDMODE_ALPHA;

	//PE: Create root.
	Entity root;
	bool bUsePrevRoot = false;
	if (proot > 0)
	{
		root = proot;
		bUsePrevRoot = true;
	}
	else
	{
		root = CreateEntity();
		scene.transforms.Create(root);
		scene.layers.Create(root).layerMask = ~0;
	}

	if (!bUsePrevRoot)
	{
		//PE: Parent all unparented transforms to new root entity
		for (size_t i = 0; i < scene.transforms.GetCount() - 1; ++i) // GetCount() - 1 because the last added was the "root"
		{
			Entity entity = scene.transforms.GetEntity(i);
			if (!scene.hierarchy.Contains(entity))
			{
				scene.Component_Attach(entity, root);
			}
		}
		//PE: The root component is transformed, scene is updated:
		scene.transforms.GetComponent(root)->MatrixTransform(transformMatrix);
		scene.Update(0);
	}
	GetScene().Merge(scene);

	//PE: Find name;
	wiScene::Scene& sceneR = wiScene::GetScene();

	for (int i = 0; i < sceneR.emitters.GetCount(); i++)
	{

		Entity emitter = sceneR.emitters.GetEntity(i);
		Entity text = sceneR.names.GetIndex(emitter);
		if (text > 0)
		{
			if (sceneR.names[text].name == name)
			{
				entity = emitter;
				break;
			}
		}
	}

	if (bUsePrevRoot)
	{
		sceneR.Component_Attach(entity, root);
		wiScene::TransformComponent* pTransform = wiScene::GetScene().transforms.GetComponent(entity);
		pTransform->ClearTransform();
		pTransform->Translate(position);
		pTransform->UpdateTransform();
	}

	return entity;
}

uint32_t GetVisibleWEmitters( void )
{
	uint32_t total_visible = 0;
	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity emitter = scene.emitters.GetEntity(i);
		wiEmittedParticle& ec = scene.emitters[i];
		// GGMAX 2.00: restored - this counter was reporting the TOTAL emitter count,
		// not the visible one, so editor/perf readouts over-reported.
		if (!ec.IsVisible())
			continue;
		if (!ec.IsActive())
			continue;
		total_visible++;
	}
	return total_visible;
}

#endif


void WickedCall_SetShaderParameter(int obj, int parameter , float value)
{
	sObject* GetObjectData(int iID);
	sObject* pObject = GetObjectData(obj);
	if (pObject)
	{
		for (int i = 0; i < pObject->iMeshCount; i++)
		{
			sMesh* mesh = pObject->ppMeshList[i];

			if (mesh)
			{
				wiScene::MeshComponent* meshComponent = wiScene::GetScene().meshes.GetComponent(mesh->wickedmeshindex);

				if (meshComponent)
				{
					// get material settings from mesh material or WEMaterial
					uint64_t materialEntity = meshComponent->subsets[0].materialID;
					wiScene::MaterialComponent* pMeshMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pMeshMaterial->customShaderID >= 0)
					{
						bool bChanged = false;
						// TODO: customShaderParam1-7 removed from MaterialComponent, replaced with uint4 userdata
						//if (parameter == 1)
						//{
						//	if (value != pMeshMaterial->customShaderParam1)
						//	{
						//		pMeshMaterial->customShaderParam1 = value;
						//		bChanged = true;
						//	}
						//}
						//if (parameter == 2)
						//{
						//	if (value != pMeshMaterial->customShaderParam2)
						//	{
						//		pMeshMaterial->customShaderParam2 = value;
						//		bChanged = true;
						//	}
						//}
						//if (parameter == 3)
						//{
						//	if (value != pMeshMaterial->customShaderParam3)
						//	{
						//		pMeshMaterial->customShaderParam3 = value;
						//		bChanged = true;
						//	}
						//}
						//if (parameter == 4)
						//{
						//	if (value != pMeshMaterial->customShaderParam4)
						//	{
						//		pMeshMaterial->customShaderParam4 = value;
						//		bChanged = true;
						//	}
						//}
						//if (parameter == 5)
						//{
						//	if (value != pMeshMaterial->customShaderParam5)
						//	{
						//		pMeshMaterial->customShaderParam5 = value;
						//		bChanged = true;
						//	}
						//}
						//if (parameter == 6)
						//{
						//	if (value != pMeshMaterial->customShaderParam6)
						//	{
						//		pMeshMaterial->customShaderParam6 = value;
						//		bChanged = true;
						//	}
						//}
						//if (parameter == 7)
						//{
						//	if (value != pMeshMaterial->customShaderParam7)
						//	{
						//		pMeshMaterial->customShaderParam7 = value;
						//		bChanged = true;
						//	}
						//}
						if(bChanged)
							pMeshMaterial->SetDirty();

					}
				}
			}
		}

	}

}



// ==========================================================================================
// GGMAX 3.59: WEATHER via WPE. Rain and snow used to be the legacy "ravey" particle system -
// DBP quad objects with a CPU vertex lock and a material re-texture PER RAINDROP. Lee asked for
// them to be replaced outright by the new Wicked particle effects, NOT matched to the DX11 look.
//
// Almost nothing had to be built: the legacy .PE reader, the eight emitter actions and the
// per-frame update above all already existed, and CAMERA-FOLLOW is a property stored inside the
// .pe itself (followCamera -> ec.bFollowCamera at wickedcalls_part3.cpp:2744), handled by the
// bFollowCamera block in WickedCall_UpdateEmitters. So weather tracks the player for free.
//
// Kept deliberately in THIS file rather than M-Particles.cpp so the Wicked types stay on this
// side of the fence; the game side just calls the three entry points below.

static uint32_t g_ggWeatherRoot = 0;
// Authored emit rates, captured at load so the intensity slider scales from the ORIGINAL value
// instead of compounding its own previous result frame after frame.
static std::vector< std::pair<wiECS::Entity, float> > g_ggWeatherBaseCount;
static bool  g_ggWeatherEmitPaused = false;
static float g_ggWeatherLastScale  = -1.0f;
// GGMAX 3.59 diagnostics: what we last asked for and what LoadWPE gave back.
char     g_ggWeatherLastPath[MAX_PATH] = "";
uint32_t g_ggWeatherLastResult = 0;
int      g_ggWeatherSetCalls = 0;

void GGWeather_Clear(void)
{
	if (g_ggWeatherRoot != 0)
	{
		void DeleteEmitterEffects(uint32_t root);   // DarkLUA_part5.cpp:998
		DeleteEmitterEffects(g_ggWeatherRoot);
		g_ggWeatherRoot = 0;
	}
	g_ggWeatherBaseCount.clear();
	g_ggWeatherEmitPaused = false;
	g_ggWeatherLastScale  = -1.0f;
}

uint32_t GGWeather_GetRoot(void) { return g_ggWeatherRoot; }

// mode: 0 none, 1/2 rain, 3/4 snow, anything else none. 2 and 4 are not reachable from the live
// panel but DO appear in older saved levels, so they must map to something.
void GGWeather_Set(int mode)
{
	GGWeather_Clear();
	g_ggWeatherSetCalls++;

	const char* pe = nullptr;
	if      (mode == 1 || mode == 2) pe = "particlesbank\\wpe\\Weather Effects\\Light Rain.pe";
	else if (mode == 3 || mode == 4) pe = "particlesbank\\wpe\\Weather Effects\\Light Snow.pe";
	if (pe == nullptr) return;

	char path[MAX_PATH];
	strcpy_s(path, MAX_PATH, pe);
	uint32_t WickedCall_LoadWPE(char* filename);
	const uint32_t root = WickedCall_LoadWPE(path);
	strcpy_s(g_ggWeatherLastPath, MAX_PATH, path);
	g_ggWeatherLastResult = root;
	if (root == 0)
	{
		void timestampactivity(int i, char* desc_s);
		char dbg[MAX_PATH];
		sprintf(dbg, "GGWeather: LoadWPE FAILED for %s", pe);
		timestampactivity(0, dbg);
		return;
	}
	g_ggWeatherRoot = root;
	{
		void timestampactivity(int i, char* desc_s);
		char dbg[MAX_PATH];
		sprintf(dbg, "GGWeather: loaded %s root=%u", pe, (unsigned)root);
		timestampactivity(0, dbg);
	}

	// Capture the authored emit rate of every emitter under the root.
	Scene& scene = wiScene::GetScene();
	for (int i = 0; i < scene.emitters.GetCount(); i++)
	{
		Entity e = scene.emitters.GetEntity(i);
		HierarchyComponent* hier = scene.hierarchy.GetComponent(e);
		if (hier == nullptr || hier->parentID != (Entity)root) continue;
		wiEmittedParticle* ec = scene.emitters.GetComponent(e);
		if (ec != nullptr) g_ggWeatherBaseCount.push_back(std::make_pair(e, ec->count));
	}

	// WickedCall_LoadWPE hides only the LAST emitter it loaded and restarts only that one; both
	// weather effects define TWO. PerformEmitterAction walks every emitter under the root, so
	// driving it from here covers them all.
	WickedCall_PerformEmitterAction(4, root);   // Restart
	WickedCall_PerformEmitterAction(8, root);   // resume emitting
	WickedCall_PerformEmitterAction(5, root);   // visible
}

// Per frame. intensityPercent is t.visuals.fWeatherIntensity (0..100); bIndoors comes from the
// game-side upward raycast.
void GGWeather_Update(float intensityPercent, bool bIndoors)
{
	if (g_ggWeatherRoot == 0) return;

	// Indoors: PAUSE EMISSION rather than zeroing the particle count. Emit-pause lets the drops
	// already in the air fall and time out; zeroing the count would delete them mid-flight and
	// the weather would vanish the instant you stepped under cover.
	if (bIndoors != g_ggWeatherEmitPaused)
	{
		WickedCall_PerformEmitterAction(bIndoors ? 7 : 8, g_ggWeatherRoot);
		g_ggWeatherEmitPaused = bIndoors;
	}
	if (bIndoors) return;

	float scale = intensityPercent / 100.0f;
	if (scale < 0.0f) scale = 0.0f;
	if (scale > 1.0f) scale = 1.0f;
	if (scale == g_ggWeatherLastScale) return;   // only touch the components when it actually moves
	g_ggWeatherLastScale = scale;

	// Scale the EMIT RATE (count), never SetMaxParticleCount - that blanks counterBuffer and
	// forces a synchronous fence-and-recreate of eleven GPU buffers, every time the slider moves.
	Scene& scene = wiScene::GetScene();
	for (size_t i = 0; i < g_ggWeatherBaseCount.size(); i++)
	{
		wiEmittedParticle* ec = scene.emitters.GetComponent(g_ggWeatherBaseCount[i].first);
		if (ec != nullptr) ec->count = g_ggWeatherBaseCount[i].second * scale;
	}
}
