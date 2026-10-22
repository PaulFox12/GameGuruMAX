void WickedCall_TextureMesh(sMesh* pMesh)
{
	if (pMesh)
	{
		if (g_bWickedUseImagePtrInsteadOfTexFile == true)
		{
			// uses special way to texture a wicked object, using the old DX11 texture image ptr
			WickedCall_TextureMeshWithImagePtr(pMesh,g_iWickedPutInEmissiveMode);
		}
		else
		{
			// get wicked meshID
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				// special flag which will wipe out texture info
				if (g_bWickedIgnoreTextureInfo == true)
				{
					// used when we do NOT want textures from loaded objects, we have a wicked new approach for that object :)
					for( int i = 0; i < pMesh->dwTextureCount; i++ ) strcpy(pMesh->pTextures[i].pName, "");
				}

				// get mesh texture filename
				char* pTextureFilename = pMesh->pTextures[0].pName;

				//PE: check if we can find a multimaterial textures.
				if (pTextureFilename && strlen(pTextureFilename) < 1)
				{
					if (pMesh && pMesh->bUseMultiMaterial)
					{
						if (pMesh->pMultiMaterial && pMesh->pMultiMaterial[0].pName)
						{
							pTextureFilename = pMesh->pMultiMaterial[0].pName;
						}
					}
				}
				std::string sTextureFilenameBase = pTextureFilename;

				// get material from mesh
				uint64_t materialEntity = mesh->subsets[0].materialID;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);

				// set DEFAULT RAW material values
				pObjectMaterial->baseColor = XMFLOAT4(1, 1, 1, 1);

				// if surface texture specified, but ROUGH,METAL,AO empty, fill them as no reference to preserve at this point
				if (pMesh->dwTextureCount >= GG_MESH_TEXTURE_SURFACE && strlen(pMesh->pTextures[GG_MESH_TEXTURE_SURFACE].pName) != 0)
				{
					if (strlen(pMesh->pTextures[GG_MESH_TEXTURE_ROUGHNESS].pName) == 0)
					{
						strcpy (pMesh->pTextures[GG_MESH_TEXTURE_ROUGHNESS].pName, pMesh->pTextures[GG_MESH_TEXTURE_SURFACE].pName);
						pMesh->pTextures[GG_MESH_TEXTURE_ROUGHNESS].channelMask = (15)+(1 << 4);
					}
					if (strlen(pMesh->pTextures[GG_MESH_TEXTURE_METALNESS].pName) == 0)
					{
						strcpy (pMesh->pTextures[GG_MESH_TEXTURE_METALNESS].pName, pMesh->pTextures[GG_MESH_TEXTURE_SURFACE].pName);
						pMesh->pTextures[GG_MESH_TEXTURE_METALNESS].channelMask = (15)+(2 << 4);
					}
					if (strlen(pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].pName) == 0)
					{
						strcpy (pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].pName, pMesh->pTextures[GG_MESH_TEXTURE_SURFACE].pName);
						pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].channelMask = (15)+(0 << 4);
					}
				}

				// set roughness strength if have texture
				pObjectMaterial->roughness = 0.0f;
				if ( pMesh->dwTextureCount > GG_MESH_TEXTURE_ROUGHNESS && strlen(pMesh->pTextures[GG_MESH_TEXTURE_ROUGHNESS].pName)!=0 )
					pObjectMaterial->roughness = 1.0f;

				// set metalness strength if have texture
				pObjectMaterial->metalness = 0.0f;
				if ( pMesh->dwTextureCount > GG_MESH_TEXTURE_METALNESS && strlen(pMesh->pTextures[GG_MESH_TEXTURE_METALNESS].pName)!=0 )
					pObjectMaterial->metalness = 1.0f;

				// set roughness strength if have texture
				pObjectMaterial->emissiveColor.w = 0.0f;
				if (pMesh->dwTextureCount > GG_MESH_TEXTURE_EMISSIVE && strlen(pMesh->pTextures[GG_MESH_TEXTURE_EMISSIVE].pName) != 0)
					pObjectMaterial->emissiveColor.w = 0.0f; // 64.0f; corrected default as it was before, no emissive strength until set
				
				// default reflectance
				pObjectMaterial->reflectance = 0.04f;// 0.002f;

				//Make sure Trees And Veg get alpharef.
				float fAlphaRef = WickedGetTreeAlphaRef();
				pObjectMaterial->alphaRef = fAlphaRef;

				// split passed in texture path/name
				std::string sFoundTexturePath = "";
				std::string sFoundTextureFilename = sTextureFilenameBase;
				std::string sFoundTextureType = "";
				char pSplitTextureFilenameBase[MAX_PATH];
				strcpy(pSplitTextureFilenameBase, sTextureFilenameBase.c_str());
				for (int n = strlen(pSplitTextureFilenameBase); n > 0; n--)
				{
					if (pSplitTextureFilenameBase[n] == '\\' || pSplitTextureFilenameBase[n] == '/')
					{
						sFoundTextureFilename = pSplitTextureFilenameBase + n + 1;
						pSplitTextureFilenameBase[n + 1] = 0;
						sFoundTexturePath = pSplitTextureFilenameBase;
						break;
					}
				}
				strcpy(pSplitTextureFilenameBase, sFoundTextureFilename.c_str());
				for (int n = strlen(pSplitTextureFilenameBase); n > 0; n--)
				{
					if (pSplitTextureFilenameBase[n] == '.')
					{
						sFoundTextureType = pSplitTextureFilenameBase + n;
						pSplitTextureFilenameBase[n] = 0;
						sFoundTextureFilename = pSplitTextureFilenameBase;
						break;
					}
				}

				// will be using "FileExistPrefDDS" as file check which does a second check for DDS (ideal for optimized standalones)
				bool bFoundTextureToLoad = false;
				std::string sFoundFinalPathAndFilename = "";
				std::string sTextureFileName;
				bool bWickedMaterialActive = IsWickedMaterialActive(pMesh);
				if (bWickedMaterialActive)
				{
					// Use wicked material from fpe.
					bool bFound = false;
					//PE: Optimize by hit rate.
					cstr sBaseColor = WickedGetBaseColorName();
					if (sFoundTexturePath.size() <= 0)
					{
						//PE: Better hit rate.
						sFoundFinalPathAndFilename = g_pWickedTexturePath + sBaseColor.Get();
						if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
						{
							sFoundFinalPathAndFilename = sFoundTexturePath + sBaseColor.Get();
							if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
								sFoundFinalPathAndFilename = sBaseColor.Get();
							else
								bFound = true;
						}
						else
							bFound = true;
					}
					else
					{
						sFoundFinalPathAndFilename = sFoundTexturePath + sBaseColor.Get();
						if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
						{
							sFoundFinalPathAndFilename = g_pWickedTexturePath + sBaseColor.Get();
							if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
								sFoundFinalPathAndFilename = sBaseColor.Get();
							else
								bFound = true;
						}
						else
							bFound = true;
					}
					if (bFound || WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 1)
					{
						if (pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource.IsValid()) //PE: Delete first if already active.
						{
							pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource = {};
							pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = "";
							pObjectMaterial->SetDirty();
							wiJobSystem::context ctx;
							wiJobSystem::Wait(ctx);
						}

						pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = sFoundFinalPathAndFilename;
						pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name);
						if (!pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource.IsValid())
						{
							//PE: If prefer dds and got png in filename it fails, try dds version.
							char texturename[MAX_PATH];
							strcpy(texturename, sFoundFinalPathAndFilename.c_str());
							int iLen = strlen(texturename);
							if (iLen > 4 &&
								texturename[iLen - 3] == 'p' ||
								texturename[iLen - 3] == 'P')
							{
								texturename[iLen - 3] = 'd';
								texturename[iLen - 2] = 'd';
								texturename[iLen - 1] = 's';
								pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = texturename;
								pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name);
							}
						}
						if (pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource.IsValid())
						{
							//PE: save full path as g_pWickedTexturePath is lost later.
							strcpy(pMesh->pTextures[0].pName, sFoundFinalPathAndFilename.c_str());

							//TODO: Get AlphaRef.
							float fAlphaRef = WickedGetAlphaRef();
							if (fAlphaRef < 0.0) fAlphaRef = 1.0f;
							pObjectMaterial->SetAlphaRef(fAlphaRef);

							//Normalmap.
							if (sFoundTexturePath.size() <= 0)
							{
								//PE: Best hit rate.
								sFoundFinalPathAndFilename = g_pWickedTexturePath + WickedGetNormalName().Get();
								if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
								{
									sFoundFinalPathAndFilename = WickedGetNormalName().Get();
								}
							}
							else
							{
								sFoundFinalPathAndFilename = sFoundTexturePath + WickedGetNormalName().Get();
								if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
								{
									sFoundFinalPathAndFilename = g_pWickedTexturePath + WickedGetNormalName().Get();
									if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
										sFoundFinalPathAndFilename = WickedGetNormalName().Get();
								}
							}
							if (pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource.IsValid()) //PE: Delete first if already active.
							{
								pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = {};
								pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = "";
								pObjectMaterial->SetDirty();
								wiJobSystem::context ctx;
								wiJobSystem::Wait(ctx);
							}

							pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = sFoundFinalPathAndFilename;
							pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::NORMALMAP].name);
							if (!pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource.IsValid())
							{
								//PE: If prefer dds and got png in filename it fails, try dds version.
								char texturename[MAX_PATH];
								strcpy(texturename, sFoundFinalPathAndFilename.c_str());
								int iLen = strlen(texturename);
								if (iLen > 4 &&
									texturename[iLen - 3] == 'p' ||
									texturename[iLen - 3] == 'P')
								{
									texturename[iLen - 3] = 'd';
									texturename[iLen - 2] = 'd';
									texturename[iLen - 1] = 's';
									pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = texturename;
									pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::NORMALMAP].name);
								}
							}

							if (pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource.IsValid())
							{
								//Set normal intensity.
								pObjectMaterial->SetNormalMapStrength(WickedGetNormalStrength());
							}
							else
							{
								pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = ""; //Prevent wicked from reloading image.
							}

							//Surface.
							if (sFoundTexturePath.size() <= 0)
							{
								//PE: Best hit rate.
								sFoundFinalPathAndFilename = g_pWickedTexturePath + WickedGetSurfaceName().Get();
								if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
								{
									sFoundFinalPathAndFilename = WickedGetSurfaceName().Get();
								}
							}
							else
							{
								sFoundFinalPathAndFilename = sFoundTexturePath + WickedGetSurfaceName().Get();
								if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
								{
									sFoundFinalPathAndFilename = g_pWickedTexturePath + WickedGetSurfaceName().Get();
									if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
										sFoundFinalPathAndFilename = WickedGetSurfaceName().Get();
								}
							}

							if (pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid()) //PE: Delete first if already active.
							{
								pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = {};
								pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = "";
								pObjectMaterial->SetDirty();
								wiJobSystem::context ctx;
								wiJobSystem::Wait(ctx);
							}

							pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = sFoundFinalPathAndFilename;
							pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name);
							if (!pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid())
							{
								//PE: If prefer dds and got png in filename it fails, try dds version.
								char texturename[MAX_PATH];
								strcpy(texturename, sFoundFinalPathAndFilename.c_str());
								int iLen = strlen(texturename);
								if (iLen > 4 &&
									texturename[iLen - 3] == 'p' ||
									texturename[iLen - 3] == 'P')
								{
									texturename[iLen - 3] = 'd';
									texturename[iLen - 2] = 'd';
									texturename[iLen - 1] = 's';
									pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = texturename;
									pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name);
								}
							}

							if (pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid())
							{
								//Set roughness,metalness intensity.
								pObjectMaterial->SetRoughness(WickedGetRoughnessStrength());
								pObjectMaterial->SetMetalness(WickedGetMetallnessStrength());

								// also enable AO from surface map
								pObjectMaterial->SetOcclusionEnabled_Primary(true);
								pObjectMaterial->SetOcclusionEnabled_Secondary(false);
							}
							else
							{
								pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = "";
							}

							// Parallax Occlusion Mapping (if HEIGHT TEXTURE used)
							bool bPOMShaderRequired = false;
							sFoundFinalPathAndFilename = sFoundTexturePath + WickedGetDisplacementName().Get();
							if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
							{
								sFoundFinalPathAndFilename = g_pWickedTexturePath + WickedGetDisplacementName().Get();
								if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
									sFoundFinalPathAndFilename = WickedGetDisplacementName().Get();
							}
							if (pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].resource.IsValid())
							{
								pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].resource = {};
								pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].name = "";
								pObjectMaterial->SetDirty();
								wiJobSystem::context ctx;
								wiJobSystem::Wait(ctx);
							}

							pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].name = sFoundFinalPathAndFilename;
							pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].name);
							if (!pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].resource.IsValid())
							{
								//PE: If prefer dds and got png in filename it fails, try dds version.
								char texturename[MAX_PATH];
								strcpy(texturename, sFoundFinalPathAndFilename.c_str());
								int iLen = strlen(texturename);
								if (iLen > 4 &&
									texturename[iLen - 3] == 'p' ||
									texturename[iLen - 3] == 'P')
								{
									texturename[iLen - 3] = 'd';
									texturename[iLen - 2] = 'd';
									texturename[iLen - 1] = 's';
									pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].name = texturename;
									pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].name);
								}
							}

							if (pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].resource.IsValid())
							{
								pObjectMaterial->parallaxOcclusionMapping = 0.05f;
								bPOMShaderRequired = true;
							}
							else
							{
								pObjectMaterial->textures[MaterialComponent::DISPLACEMENTMAP].name = "";
							}

							//Set emissive colors before map.
							DWORD dwEmmisiveColor = WickedGetEmmisiveColor();
							DWORD dwBaseColor = WickedGetBaseColor();
							if (dwBaseColor != -1)
							{
								pMesh->mMaterial.Diffuse.r = ((dwBaseColor & 0xff000000) >> 24) / 255.0f;;
								pMesh->mMaterial.Diffuse.g = ((dwBaseColor & 0x00ff0000) >> 16) / 255.0f;
								pMesh->mMaterial.Diffuse.b = ((dwBaseColor & 0x0000ff00) >> 8) / 255.0f;
								pMesh->mMaterial.Diffuse.a = (dwBaseColor & 0x000000ff) / 255.0f;
								WickedCall_SetMeshMaterial(pMesh,true);
							}

							//Emissive.
							if (sFoundTexturePath.size() <= 0)
							{
								//PE: Best hit rate.
								sFoundFinalPathAndFilename = g_pWickedTexturePath + WickedGetEmissiveName().Get();
								if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
								{
									//PE: Check not needed sFoundTexturePath empty.
									sFoundFinalPathAndFilename = WickedGetEmissiveName().Get();
								}
							}
							else
							{
								//PE: Get a crash here ?
								std::string ename = WickedGetEmissiveName().Get();
								sFoundFinalPathAndFilename = sFoundTexturePath;
								sFoundFinalPathAndFilename = sFoundFinalPathAndFilename + ename;
								if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
								{
									sFoundFinalPathAndFilename = g_pWickedTexturePath + WickedGetEmissiveName().Get();
									if (WickedCall_TextureFileExistPrefDDS((LPSTR)sFoundFinalPathAndFilename.c_str()) == 0)
										sFoundFinalPathAndFilename = WickedGetEmissiveName().Get();
								}
							}
							if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid()) //PE: Delete first if already active.
							{
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = {};
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
								pObjectMaterial->SetDirty();
								wiJobSystem::context ctx;
								wiJobSystem::Wait(ctx);
							}

							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sFoundFinalPathAndFilename;
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);
							float fEmissive = WickedGetEmissiveStrength();
							if (!pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
							{
								//PE: If prefer dds and got png in filename it fails, try dds version.
								char texturename[MAX_PATH];
								strcpy(texturename, sFoundFinalPathAndFilename.c_str());
								int iLen = strlen(texturename);
								if (iLen > 4 &&
									texturename[iLen - 3] == 'p' ||
									texturename[iLen - 3] == 'P')
								{
									texturename[iLen - 3] = 'd';
									texturename[iLen - 2] = 'd';
									texturename[iLen - 1] = 's';
									pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = texturename;
									pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);
								}
							}

							if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
							{
								//Set Emissive intensity.
								pObjectMaterial->SetEmissiveStrength(fEmissive);

								//Change default emmisive color if needed.
								if (dwEmmisiveColor == -1)
								{
									pMesh->mMaterial.Emissive.r = 1.0f;
									pMesh->mMaterial.Emissive.g = 1.0f;
									pMesh->mMaterial.Emissive.b = 1.0f;
									WickedCall_SetMeshMaterial(pMesh, false);
								}

							}
							else
							{
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
							}

							//PE: Moved here , We cant setup Emissive color before Emissive texture.
							//PE: Always use dwEmmisiveColor if bWickedMaterialActive.
							{
								pMesh->mMaterial.Emissive.r = ((dwEmmisiveColor & 0xff000000) >> 24) / 255.0f;;
								pMesh->mMaterial.Emissive.g = ((dwEmmisiveColor & 0x00ff0000) >> 16) / 255.0f;
								pMesh->mMaterial.Emissive.b = ((dwEmmisiveColor & 0x0000ff00) >> 8) / 255.0f;
								pMesh->mMaterial.Emissive.a = (dwEmmisiveColor & 0x000000ff) / 255.0f;

								WickedCall_SetMeshMaterial(pMesh, false);
							}

							//PE: Special object settings.
							bool bTransparent = WickedGetTransparent();
							if (bTransparent)
							{
								pObjectMaterial->userBlendMode = BLENDMODE_ALPHA;
								pObjectMaterial->SetDirty();
							}
							else
							{
								pObjectMaterial->userBlendMode = BLENDMODE_OPAQUE;
								pObjectMaterial->SetDirty();
							}

							bool bDoubleSided = WickedDoubleSided();
							if (bDoubleSided)
								mesh->SetDoubleSided(true);
							else
								mesh->SetDoubleSided(false);

							float fRenderOrderBias = WickedRenderOrderBias();
							WickedCall_SetRenderOrderBias(pMesh, fRenderOrderBias);

							int iCustomShaderID = WickedCustomShaderID();
							bool bValid = true;
							if (iCustomShaderID == 5 && (bDoubleSided || pestrcasestr(pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name.c_str(), "eyeglasses") )) //|| pObjectMaterial->GetBlendMode() == BLENDMODE_ALPHA)
							{
								pObjectMaterial->customShaderID = -1;
								bValid = false;
							}
							if (bValid)
							{
								float iCustomShaderParam1 = WickedCustomShaderParam1();
								float iCustomShaderParam2 = WickedCustomShaderParam2();
								float iCustomShaderParam3 = WickedCustomShaderParam3();
								float iCustomShaderParam4 = WickedCustomShaderParam4();
								float iCustomShaderParam5 = WickedCustomShaderParam5();
								float iCustomShaderParam6 = WickedCustomShaderParam6();
								float iCustomShaderParam7 = WickedCustomShaderParam7();
								pObjectMaterial->customShaderID = iCustomShaderID;
								// GGMAX 3.58b: ENTITY TREE SWAY. customShaderID 1 is "Tree Animate Doublesided" (the engine
								// registers its own Hologram shader at 0, so GameGuru's first registration lands on 1).
								// DX11 swayed these through that custom shader using the per-material "Object Wind" value,
								// and crucially did so EVEN WITH visuals.tree_wind = 0 - which is why DX11 levels sway with
								// the global slider at zero, and why they went still in DX12: that shader silently compiles
								// against the ENGINE objectHF (no TreeWave in it), so objectVS_common_tree.cso is
								// bit-identical to the stock shader. Rather than revive a Wicked-0.60-era shader that will
								// not compile against 0.71, hand the authored value to the engine wind path that 3.57 built.
								//
								// The value itself survived the port untouched - it is parsed from the FPE, read from level
								// data and written back on save. Only the hand-off to the material was lost (the commented
								// customShaderParam1 lines just below). userdata.x is free: nothing else in the engine or
								// the game reads or writes it.
								//
								// param1 == 0 deliberately leaves wind OFF rather than on-at-zero: an entity mesh has no
								// vertex_windweights, so the engine would substitute 1.0 for every vertex and translate the
								// whole tree rigidly, base and all.
								if (iCustomShaderID == 1 && iCustomShaderParam1 > 0.0f)
								{
									pObjectMaterial->userdata.x = *reinterpret_cast<const uint32_t*>(&iCustomShaderParam1);
									pObjectMaterial->SetUseWind(true);
								}
								else
								{
									pObjectMaterial->userdata.x = 0;
								}
								// TODO: customShaderParam1-7 removed from MaterialComponent, replaced with uint4 userdata
								//pObjectMaterial->customShaderParam1 = iCustomShaderParam1;
								//pObjectMaterial->customShaderParam2 = iCustomShaderParam2;
								//pObjectMaterial->customShaderParam3 = iCustomShaderParam3;
								//pObjectMaterial->customShaderParam4 = iCustomShaderParam4;
								//pObjectMaterial->customShaderParam5 = iCustomShaderParam5;
								//pObjectMaterial->customShaderParam6 = iCustomShaderParam6;
								//pObjectMaterial->customShaderParam7 = iCustomShaderParam7;
							}
							bool bPlanerReflection = WickedPlanerReflection();
							if (bPlanerReflection)
							{
								pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR_PLANARREFLECTION;
							}
							else
							{
								if (pObjectMaterial->parallaxOcclusionMapping > 0.0f)
									pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING;
								else
									pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR;
							}

							bool bCastShadows = WickedGetCastShadows();
							if (bCastShadows)
							{
								pObjectMaterial->SetCastShadow(true);
							}
							else
							{
								pObjectMaterial->SetCastShadow(false);
							}

							float fReflectance = WickedGetReflectance();
							pObjectMaterial->SetReflectance(fReflectance);

							pObjectMaterial->SetDirty();

						}
						else
						{
							//PE: Use alpha ref from material.
							float fAlphaRef = WickedGetAlphaRef();
							if (fAlphaRef < 0.0) fAlphaRef = 1.0f;
							pObjectMaterial->SetAlphaRef(fAlphaRef);

							//Wicked material failed  , use old way.
							pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = ""; //Prevent wicked form trying to reload image.
							bWickedMaterialActive = false;

							//PE: Apply special settings even if not wicked material texture is set.
							bool bTransparent = WickedGetTransparent();
							if (bTransparent)
							{
								pObjectMaterial->userBlendMode = BLENDMODE_ALPHA;
								pObjectMaterial->SetDirty();
							}
							else
							{
								pObjectMaterial->userBlendMode = BLENDMODE_OPAQUE;
								pObjectMaterial->SetDirty();
							}

							bool bDoubleSided = WickedDoubleSided();
							if (bDoubleSided)
								mesh->SetDoubleSided(true);
							else
								mesh->SetDoubleSided(false);

							float fRenderOrderBias = WickedRenderOrderBias();
							WickedCall_SetRenderOrderBias(pMesh, fRenderOrderBias);

							int iCustomShaderID = WickedCustomShaderID();
							bool bValid = true;
							if (iCustomShaderID == 5 && (bDoubleSided || pestrcasestr(pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name.c_str(), "eyeglasses") )) //|| pObjectMaterial->GetBlendMode() == BLENDMODE_ALPHA
							{
								pObjectMaterial->customShaderID = -1;
								bValid = false;
							}
							if (bValid)
							{
								float iCustomShaderParam1 = WickedCustomShaderParam1();
								float iCustomShaderParam2 = WickedCustomShaderParam2();
								float iCustomShaderParam3 = WickedCustomShaderParam3();
								float iCustomShaderParam4 = WickedCustomShaderParam4();
								float iCustomShaderParam5 = WickedCustomShaderParam5();
								float iCustomShaderParam6 = WickedCustomShaderParam6();
								float iCustomShaderParam7 = WickedCustomShaderParam7();
								pObjectMaterial->customShaderID = iCustomShaderID;
								// GGMAX 3.58b: ENTITY TREE SWAY. customShaderID 1 is "Tree Animate Doublesided" (the engine
								// registers its own Hologram shader at 0, so GameGuru's first registration lands on 1).
								// DX11 swayed these through that custom shader using the per-material "Object Wind" value,
								// and crucially did so EVEN WITH visuals.tree_wind = 0 - which is why DX11 levels sway with
								// the global slider at zero, and why they went still in DX12: that shader silently compiles
								// against the ENGINE objectHF (no TreeWave in it), so objectVS_common_tree.cso is
								// bit-identical to the stock shader. Rather than revive a Wicked-0.60-era shader that will
								// not compile against 0.71, hand the authored value to the engine wind path that 3.57 built.
								//
								// The value itself survived the port untouched - it is parsed from the FPE, read from level
								// data and written back on save. Only the hand-off to the material was lost (the commented
								// customShaderParam1 lines just below). userdata.x is free: nothing else in the engine or
								// the game reads or writes it.
								//
								// param1 == 0 deliberately leaves wind OFF rather than on-at-zero: an entity mesh has no
								// vertex_windweights, so the engine would substitute 1.0 for every vertex and translate the
								// whole tree rigidly, base and all.
								if (iCustomShaderID == 1 && iCustomShaderParam1 > 0.0f)
								{
									pObjectMaterial->userdata.x = *reinterpret_cast<const uint32_t*>(&iCustomShaderParam1);
									pObjectMaterial->SetUseWind(true);
								}
								else
								{
									pObjectMaterial->userdata.x = 0;
								}
								// TODO: customShaderParam1-7 removed from MaterialComponent, replaced with uint4 userdata
								//pObjectMaterial->customShaderParam1 = iCustomShaderParam1;
								//pObjectMaterial->customShaderParam2 = iCustomShaderParam2;
								//pObjectMaterial->customShaderParam3 = iCustomShaderParam3;
								//pObjectMaterial->customShaderParam4 = iCustomShaderParam4;
								//pObjectMaterial->customShaderParam5 = iCustomShaderParam5;
								//pObjectMaterial->customShaderParam6 = iCustomShaderParam6;
								//pObjectMaterial->customShaderParam7 = iCustomShaderParam7;
							}
							bool bPlanerReflection = WickedPlanerReflection();
							if (bPlanerReflection)
							{
								pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR_PLANARREFLECTION;
							}
							else
							{
								if (pObjectMaterial->parallaxOcclusionMapping > 0.0f)
									pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING;
								else
									pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR;
							}

							bool bCastShadows = WickedGetCastShadows();
							if (bCastShadows)
							{
								pObjectMaterial->SetCastShadow(true);
							}
							else
							{
								pObjectMaterial->SetCastShadow(false);
							}

							float fReflectance = WickedGetReflectance();
							pObjectMaterial->SetReflectance(fReflectance);

							pObjectMaterial->SetDirty();

						}
					}
					else
					{
						//PE: Use alpha ref from material.
						float fAlphaRef = WickedGetAlphaRef();
						if (fAlphaRef < 0.0) fAlphaRef = 1.0f;
						pObjectMaterial->SetAlphaRef(fAlphaRef);

						//Wicked material failed  , use old way.
						bWickedMaterialActive = false;

						//PE: Apply special settings even if not wicked material texture is set.
						bool bTransparent = WickedGetTransparent();
						if (bTransparent)
						{
							pObjectMaterial->userBlendMode = BLENDMODE_ALPHA;
							pObjectMaterial->SetDirty();
						}
						else
						{
							pObjectMaterial->userBlendMode = BLENDMODE_OPAQUE;
							pObjectMaterial->SetDirty();
						}

						bool bDoubleSided = WickedDoubleSided();
						if (bDoubleSided)
							mesh->SetDoubleSided(true);
						else
							mesh->SetDoubleSided(false);

						float fRenderOrderBias = WickedRenderOrderBias();
						WickedCall_SetRenderOrderBias(pMesh, fRenderOrderBias);

						int iCustomShaderID = WickedCustomShaderID();
						bool bValid = true;
						if (iCustomShaderID == 5 && (bDoubleSided || pestrcasestr(pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name.c_str(), "eyeglasses") )) //|| pObjectMaterial->GetBlendMode() == BLENDMODE_ALPHA
						{
							pObjectMaterial->customShaderID = -1;
							bValid = false;
						}
						if (bValid)
						{
							float iCustomShaderParam1 = WickedCustomShaderParam1();
							float iCustomShaderParam2 = WickedCustomShaderParam2();
							float iCustomShaderParam3 = WickedCustomShaderParam3();
							float iCustomShaderParam4 = WickedCustomShaderParam4();
							float iCustomShaderParam5 = WickedCustomShaderParam5();
							float iCustomShaderParam6 = WickedCustomShaderParam6();
							float iCustomShaderParam7 = WickedCustomShaderParam7();
							pObjectMaterial->customShaderID = iCustomShaderID;
							// GGMAX 3.58b: ENTITY TREE SWAY. customShaderID 1 is "Tree Animate Doublesided" (the engine
							// registers its own Hologram shader at 0, so GameGuru's first registration lands on 1).
							// DX11 swayed these through that custom shader using the per-material "Object Wind" value,
							// and crucially did so EVEN WITH visuals.tree_wind = 0 - which is why DX11 levels sway with
							// the global slider at zero, and why they went still in DX12: that shader silently compiles
							// against the ENGINE objectHF (no TreeWave in it), so objectVS_common_tree.cso is
							// bit-identical to the stock shader. Rather than revive a Wicked-0.60-era shader that will
							// not compile against 0.71, hand the authored value to the engine wind path that 3.57 built.
							//
							// The value itself survived the port untouched - it is parsed from the FPE, read from level
							// data and written back on save. Only the hand-off to the material was lost (the commented
							// customShaderParam1 lines just below). userdata.x is free: nothing else in the engine or
							// the game reads or writes it.
							//
							// param1 == 0 deliberately leaves wind OFF rather than on-at-zero: an entity mesh has no
							// vertex_windweights, so the engine would substitute 1.0 for every vertex and translate the
							// whole tree rigidly, base and all.
							if (iCustomShaderID == 1 && iCustomShaderParam1 > 0.0f)
							{
								pObjectMaterial->userdata.x = *reinterpret_cast<const uint32_t*>(&iCustomShaderParam1);
								pObjectMaterial->SetUseWind(true);
							}
							else
							{
								pObjectMaterial->userdata.x = 0;
							}
							// TODO: customShaderParam1-7 removed from MaterialComponent, replaced with uint4 userdata
							//pObjectMaterial->customShaderParam1 = iCustomShaderParam1;
							//pObjectMaterial->customShaderParam2 = iCustomShaderParam2;
							//pObjectMaterial->customShaderParam3 = iCustomShaderParam3;
							//pObjectMaterial->customShaderParam4 = iCustomShaderParam4;
							//pObjectMaterial->customShaderParam5 = iCustomShaderParam5;
							//pObjectMaterial->customShaderParam6 = iCustomShaderParam6;
							//pObjectMaterial->customShaderParam7 = iCustomShaderParam7;
						}

						bool bPlanerReflection = WickedPlanerReflection();
						if (bPlanerReflection)
						{
							pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR_PLANARREFLECTION;
						}
						else
						{
							if (pObjectMaterial->parallaxOcclusionMapping > 0.0f)
								pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING;
							else
								pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR;
						}

						bool bCastShadows = WickedGetCastShadows();
						if (bCastShadows)
						{
							pObjectMaterial->SetCastShadow(true);
						}
						else
						{
							pObjectMaterial->SetCastShadow(false);
						}

						float fReflectance = WickedGetReflectance();
						pObjectMaterial->SetReflectance(fReflectance);

						pObjectMaterial->SetDirty();

					}
				}

				if (!bWickedMaterialActive)
				{
					// skip normal, surface and emissive PBR Ready Constructions if already loaded in
					bool bGotNormalTexture = false;
					bool bGotSurfaceTexture = false;
					bool bGotEmissiveTexture = false;

					if (pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource.IsValid()) //PE: Delete first if already active.
					{
						pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource = {};
						pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = "";
						pObjectMaterial->SetDirty();
						wiJobSystem::context ctx;
						wiJobSystem::Wait(ctx);
					}

					// go through all permutations until we find the texture
					for (int findit = 0; findit <= 5; findit++)
					{
						if (findit == 0) { sFoundFinalPathAndFilename = sFoundTexturePath + sFoundTextureFilename + sFoundTextureType; }
						if (findit == 1) { sFoundFinalPathAndFilename = g_pWickedTexturePath + sFoundTextureFilename + sFoundTextureType; }
						if (findit == 2) { sFoundTextureType = ".dds"; sFoundFinalPathAndFilename = sFoundTexturePath + sFoundTextureFilename + sFoundTextureType; }
						if (findit == 3) { sFoundTextureType = ".dds"; sFoundFinalPathAndFilename = g_pWickedTexturePath + sFoundTextureFilename + sFoundTextureType; }
						if (findit == 4) { sFoundTextureType = ".png"; sFoundFinalPathAndFilename = sFoundTexturePath + sFoundTextureFilename + sFoundTextureType; }
						if (findit == 5) { sFoundTextureType = ".png"; sFoundFinalPathAndFilename = g_pWickedTexturePath + sFoundTextureFilename + sFoundTextureType; }

						pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = sFoundFinalPathAndFilename;
						pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name);
						if (pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource.IsValid())
						{
							// found the texture file location and type
							// carried in sFoundFinalPathAndFilename
							bFoundTextureToLoad = true;

							//PE: For "CloneObject" we need to update. pMesh->pTextures[0].pName
							//PE: To the full path as g_pWickedTexturePath is lost later.
							strcpy(pMesh->pTextures[0].pName, sFoundFinalPathAndFilename.c_str());

							break;
						}
						else
						{
							pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = "";
						}
					}

					if ( pMesh->dwTextureCount > GG_MESH_TEXTURE_NORMAL && *(pMesh->pTextures[GG_MESH_TEXTURE_NORMAL].pName) )
					{
						// Normal texture
						if (pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource.IsValid()) // Delete first if already active.
						{
							pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = {};
							pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = "";
							pObjectMaterial->SetDirty();
							wiJobSystem::context ctx;
							wiJobSystem::Wait(ctx);
						}
						pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = pMesh->pTextures[GG_MESH_TEXTURE_NORMAL].pName;
						if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::NORMALMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = "";
						else
						{
							pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::NORMALMAP].name);
							bGotNormalTexture = true;
						}
					}

					if ( pMesh->dwTextureCount > GG_MESH_TEXTURE_SURFACE && *(pMesh->pTextures[GG_MESH_TEXTURE_SURFACE].pName) )
					{
						// Ambient occlusion texture
						if (pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid()) // Delete first if already active.
						{
							pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = {};
							pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = "";
							pObjectMaterial->SetDirty();
							wiJobSystem::context ctx;
							wiJobSystem::Wait(ctx);
						}
						pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = pMesh->pTextures[GG_MESH_TEXTURE_SURFACE].pName;
						if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name.c_str())) 
							pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = "";
						else
						{
							pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name);
							pObjectMaterial->SetOcclusionEnabled_Primary(true);
							pObjectMaterial->SetOcclusionEnabled_Secondary(false);
							bGotSurfaceTexture = true;
						}
					}
					if ( pMesh->dwTextureCount > GG_MESH_TEXTURE_EMISSIVE && *(pMesh->pTextures[GG_MESH_TEXTURE_EMISSIVE].pName) )
					{
						// Emissive texture
						if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid()) // Delete first if already active.
						{
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = {};
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
							pObjectMaterial->SetDirty();
							wiJobSystem::context ctx;
							wiJobSystem::Wait(ctx);
						}
						pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = pMesh->pTextures[GG_MESH_TEXTURE_EMISSIVE].pName;
						if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
						else
						{
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);
							bGotEmissiveTexture = true;
						}
					}

					if ( pMesh->bTransparency )
						pObjectMaterial->userBlendMode = BLENDMODE_ALPHA;

					// default emissive is off (when not custom material)
					pMesh->mMaterial.Emissive.r = 0.0f;
					pMesh->mMaterial.Emissive.g = 0.0f;
					pMesh->mMaterial.Emissive.b = 0.0f;
					WickedCall_SetMeshMaterial(pMesh, false);

					// if found and loaded base texture, continue with rest
					if (bFoundTextureToLoad == true)
					{
						// default texture format is DDS (if not already determined)
						if (sFoundTextureType == "") sFoundTextureType = ".dds";

						// have a backup format in case PNG/DDS twizzles happening in source asset!
						std::string sAltTextureType = ".dds";
						if (sFoundTextureType == ".dds") sAltTextureType = ".png";

						// determine if filename is ready for PBR additions, or is regular 'nonPBR' full filename
						bool bPBRReady = false;
						char pDetectUnderscoreColor[MAX_PATH];
						strcpy(pDetectUnderscoreColor, sFoundFinalPathAndFilename.c_str());
						//pDetectUnderscoreColor[strlen(pDetectUnderscoreColor) - 4] = 0; // extension might be more than 4 characters (.jpeg)
						char* pExt = strrchr( pDetectUnderscoreColor, '.' );
						if ( pExt ) *pExt = 0;
						if (strnicmp(pDetectUnderscoreColor + strlen(pDetectUnderscoreColor) - 6, "_color", 6) == NULL) bPBRReady = true;

						// apply various PBR textures to mesh material
						if (bPBRReady == true)
						{
							// Remove the _color.xxx part
							char pTrimFinalTextureFilenameBase[MAX_PATH];
							strcpy(pTrimFinalTextureFilenameBase, sFoundFinalPathAndFilename.c_str());
							pTrimFinalTextureFilenameBase[strlen(pTrimFinalTextureFilenameBase) - 10] = 0;
							sTextureFilenameBase = pTrimFinalTextureFilenameBase;

							// PBR Color already loaded, what about normal
							if (bGotNormalTexture == false)
							{
								// PBR Normal texture
								if (pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource.IsValid()) //PE: Delete first if already active.
								{
									pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = {};
									pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = "";
									pObjectMaterial->SetDirty();
									wiJobSystem::context ctx;
									wiJobSystem::Wait(ctx);
								}
								pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = sTextureFilenameBase + "_normal" + sFoundTextureType;
								if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::NORMALMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = sTextureFilenameBase + "_normal" + sAltTextureType;
								pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::NORMALMAP].name);
								if (!pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource.IsValid())
								{
									pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = "";
								}
							}

							// If SURFACE texture not present, create it
							if (bGotSurfaceTexture == false && (!pObjectMaterial->IsUsingVertexColors() || pMesh->iReservedForFuture > 10 ))
							{
								// PBR Surface texture
								if (pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid()) //PE: Delete first if already active.
								{
									pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = {};
									pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = "";
									pObjectMaterial->SetDirty();
									wiJobSystem::context ctx;
									wiJobSystem::Wait(ctx);
								}
								//LB: breaks when importer model has a PNG for the surface texture when importing
								//PE: Surface must be dds, So make sure we always save as .dds, we can still merge from .png ...
								//std::string surfaceTexFile = sTextureFilenameBase + "_surface" + ".dds";
								//pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = surfaceTexFile;
								pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = sTextureFilenameBase + "_surface" + sFoundTextureType;
								if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = sTextureFilenameBase + "_surface" + sAltTextureType;
								pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name);
								std::string surfaceTexFile = "";
								if (!pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid())
								{
									// could not load DDS or PNG surface during the import, fall back and make one
									pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = "";
									surfaceTexFile = sTextureFilenameBase + "_surface" + ".dds";
								}
								LPSTR pSurfaceTexFile = (char*)surfaceTexFile.c_str();
								if (pSurfaceTexFile && strlen(pSurfaceTexFile)>0 && !WickedCall_TextureFileExist(pSurfaceTexFile))
								{
									//PE: It should save to the docwrite folder in the correct location ?.
									//PE: We have relative path here, so the newSurfaceFileTemp check dont work, without resolving it first.
									char resolve[MAX_PATH];
									strcpy(resolve, pSurfaceTexFile);
									GG_GetRealPath(resolve, 1);
									pSurfaceTexFile = resolve;

									// surface file location must be temp if not in MAX folder or GameGuruApps folder
									char newSurfaceFileTemp[MAX_PATH];
									GG_SetWritablesToRoot(true);
									strcpy(newSurfaceFileTemp, GG_GetWritePath());
									GG_SetWritablesToRoot(false);
									if (strnicmp (pSurfaceTexFile, g_rootFolder.c_str(), strlen(g_rootFolder.c_str())) != NULL
										&& strnicmp (pSurfaceTexFile, newSurfaceFileTemp, strlen(newSurfaceFileTemp)) != NULL)
									{
										// surface should not be created outside MAX folders (unwelcome)
										// so use our temporary file area (importer would then copy these when saving object)
										char fullTextureName[MAX_PATH];
										char stripTextureName[MAX_PATH];
										strcpy(stripTextureName, "import_generate");
										strcpy(fullTextureName, (char*)sTextureFilenameBase.c_str());
										for (int n = strlen(fullTextureName) - 1; n > 0; n--)
										{
											if (fullTextureName[n] == '\\' || fullTextureName[n] == '/')
											{
												strcpy(stripTextureName, fullTextureName + n + 1);
												break;
											}
										}
										strcat(newSurfaceFileTemp, "imported_models\\");
										strcat(newSurfaceFileTemp, stripTextureName);
										strcat(newSurfaceFileTemp, "_surface.dds");
										surfaceTexFile = newSurfaceFileTemp;
										pSurfaceTexFile = (char*)surfaceTexFile.c_str();
										pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = surfaceTexFile;
									}

									// create surface texture from PBR texture set provided
									std::string sTextureAO = sTextureFilenameBase + "_ao" + sFoundTextureType;
									std::string sTextureGloss = sTextureFilenameBase + "_gloss" + sFoundTextureType;
									std::string sTextureMetalness = sTextureFilenameBase + "_metalness" + sFoundTextureType;
									if (!WickedCall_TextureFileExist((char*)sTextureAO.c_str())) sTextureAO = sTextureFilenameBase + "_ao" + sAltTextureType;
									if (!WickedCall_TextureFileExist((char*)sTextureGloss.c_str())) sTextureGloss = sTextureFilenameBase + "_gloss" + sAltTextureType;
									if (!WickedCall_TextureFileExist((char*)sTextureMetalness.c_str())) sTextureMetalness = sTextureFilenameBase + "_metalness" + sAltTextureType;
									ImageCreateSurfaceTexture(pSurfaceTexFile, (char*)sTextureAO.c_str(), (char*)sTextureGloss.c_str(), (char*)sTextureMetalness.c_str());
									WickedCall_ForgetTextureFiles();

									// and assign this surface to DBO mesh along with channel info (used by importer and other code)
									if (pMesh->dwTextureCount < GG_MESH_TEXTURE_SURFACE)
									{
										//PE: Realloc we get a heap errors here.
										//PE: And are generating random crashes from everywhere.
										extern bool EnsureTextureStageValid(sMesh* pMesh, int iTextureStage);
										EnsureTextureStageValid(pMesh, GG_MESH_TEXTURE_SURFACE);
									}

									strcpy (pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].pName, pSurfaceTexFile);
									pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].channelMask = (0 << 4) + (15);
									strcpy (pMesh->pTextures[GG_MESH_TEXTURE_ROUGHNESS].pName, pSurfaceTexFile);
									pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].channelMask = (1 << 4) + (15);
									strcpy (pMesh->pTextures[GG_MESH_TEXTURE_METALNESS].pName, pSurfaceTexFile);
									pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].channelMask = (2 << 4) + (15);
								}

								// PBR Surface texture
								pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name);
								if (pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid())
								{
									pObjectMaterial->roughness = 1;
									pObjectMaterial->metalness = 1;
									pObjectMaterial->SetOcclusionEnabled_Primary(true);
									pObjectMaterial->SetOcclusionEnabled_Secondary(false);
								}
							}

							// PBR emissive
							if ( bGotEmissiveTexture == false )
							{
								if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid()) //PE: Delete first if already active.
								{
									pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = {};
									pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
									pObjectMaterial->SetDirty();
									wiJobSystem::context ctx;
									wiJobSystem::Wait(ctx);
								}
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_emissive" + sFoundTextureType;
								if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_emissive" + sAltTextureType;
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);
								if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
								{
									//PE: We need a default emissiveColor for anything to illume.
									pMesh->mMaterial.Emissive.r = 1.0f;
									pMesh->mMaterial.Emissive.g = 1.0f;
									pMesh->mMaterial.Emissive.b = 1.0f;
									pObjectMaterial->emissiveColor.x = pMesh->mMaterial.Emissive.r;
									pObjectMaterial->emissiveColor.y = pMesh->mMaterial.Emissive.g;
									pObjectMaterial->emissiveColor.z = pMesh->mMaterial.Emissive.b;

									// this ensures we do not wipe out previously set emissive strength set inside FPE!
									pObjectMaterial->emissiveColor.w = pMesh->mMaterial.Emissive.a;
									if (pObjectMaterial->emissiveColor.w == 0) pObjectMaterial->emissiveColor.w = 1.0f;
								}
								else
								{
									pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_illum" + sFoundTextureType;
									if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_illum" + sAltTextureType;
									pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);
									if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
									{
										//PE: We need a default emissiveColor for anything to illume.
										pMesh->mMaterial.Emissive.r = 1.0f;
										pMesh->mMaterial.Emissive.g = 1.0f;
										pMesh->mMaterial.Emissive.b = 1.0f;
										pObjectMaterial->emissiveColor.x = pMesh->mMaterial.Emissive.r;
										pObjectMaterial->emissiveColor.y = pMesh->mMaterial.Emissive.g;
										pObjectMaterial->emissiveColor.z = pMesh->mMaterial.Emissive.b;

										// this ensures we do not wipe out previously set emissive strength set inside FPE!
										pObjectMaterial->emissiveColor.w = pMesh->mMaterial.Emissive.a;
										if (pObjectMaterial->emissiveColor.w == 0) pObjectMaterial->emissiveColor.w = 1.0f;
									}
									else
									{
										pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_illumination" + sFoundTextureType;
										if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_illumination" + sAltTextureType;
										pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);
										if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
										{
											//PE: We need a default emissiveColor for anything to illume.
											pMesh->mMaterial.Emissive.r = 1.0f;
											pMesh->mMaterial.Emissive.g = 1.0f;
											pMesh->mMaterial.Emissive.b = 1.0f;
											pObjectMaterial->emissiveColor.x = pMesh->mMaterial.Emissive.r;
											pObjectMaterial->emissiveColor.y = pMesh->mMaterial.Emissive.g;
											pObjectMaterial->emissiveColor.z = pMesh->mMaterial.Emissive.b;

											// this ensures we do not wipe out previously set emissive strength set inside FPE!
											pObjectMaterial->emissiveColor.w = pMesh->mMaterial.Emissive.a;
											if (pObjectMaterial->emissiveColor.w == 0) pObjectMaterial->emissiveColor.w = 1.0f;
										}
										else
										{
											//Try old DNS I
											pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_i" + sFoundTextureType;
											if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_i" + sAltTextureType;
											pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);
											if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
											{
												//PE: We need a default emissiveColor for anything to illume.
												pMesh->mMaterial.Emissive.r = 1.0f;
												pMesh->mMaterial.Emissive.g = 1.0f;
												pMesh->mMaterial.Emissive.b = 1.0f;
												pObjectMaterial->emissiveColor.x = pMesh->mMaterial.Emissive.r;
												pObjectMaterial->emissiveColor.y = pMesh->mMaterial.Emissive.g;
												pObjectMaterial->emissiveColor.z = pMesh->mMaterial.Emissive.b;

												// this ensures we do not wipe out previously set emissive strength set inside FPE!
												pObjectMaterial->emissiveColor.w = pMesh->mMaterial.Emissive.a;
												if (pObjectMaterial->emissiveColor.w == 0) pObjectMaterial->emissiveColor.w = 1.0f;
											}
											else
											{
												//PE: Wicked will retry loading images if the name is set and got no resource, so clear it.
												pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = ""; //Prevent wicked from trying to reload.
											}
										}
									}
								}
							}
						}
						else
						{
							// very old DNS texture support - Diffuse sorted, but need normal and [what to do with old specular]
							bool bDNSReady = false;
							if (strnicmp(pDetectUnderscoreColor + strlen(pDetectUnderscoreColor) - 2, "_d", 2) == NULL) bDNSReady = true;
							if (bDNSReady == true)
							{
								// Remove the _D.xxx part
								char pTrimFinalTextureFilenameBase[MAX_PATH];
								strcpy(pTrimFinalTextureFilenameBase, sFoundFinalPathAndFilename.c_str());
								pTrimFinalTextureFilenameBase[strlen(pTrimFinalTextureFilenameBase) - 6] = 0;
								sTextureFilenameBase = pTrimFinalTextureFilenameBase;

								// Normal texture
								if (pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource.IsValid()) //PE: Delete first if already active.
								{
									pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = {};
									pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = "";
									pObjectMaterial->SetDirty();
									wiJobSystem::context ctx;
									wiJobSystem::Wait(ctx);
								}
								pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = sTextureFilenameBase + "_n" + sFoundTextureType;
								if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::NORMALMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = sTextureFilenameBase + "_n" + sAltTextureType;
								pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::NORMALMAP].name);
								if (!pObjectMaterial->textures[MaterialComponent::NORMALMAP].resource.IsValid())
								{
									pObjectMaterial->textures[MaterialComponent::NORMALMAP].name = "";
								}

								// Surface texture
								if (pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid()) //PE: Delete first if already active.
								{
									pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = {};
									pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = "";
									pObjectMaterial->SetDirty();
									wiJobSystem::context ctx;
									wiJobSystem::Wait(ctx);
								}
								pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = sTextureFilenameBase + "_surface" + ".dds";
								if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name.c_str()))
								{
									std::string sTextureGloss = sTextureFilenameBase + "_s" + sFoundTextureType;
									std::string sTextureMetalness = sTextureFilenameBase + "_s" + sFoundTextureType;
									if (!WickedCall_TextureFileExist((char*)sTextureGloss.c_str())) sTextureGloss = sTextureFilenameBase + "_s" + sAltTextureType;
									if (!WickedCall_TextureFileExist((char*)sTextureMetalness.c_str())) sTextureMetalness = sTextureFilenameBase + "_s" + sAltTextureType;
									LPSTR pSurfaceTexFile = (char*)pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name.c_str();
									ImageCreateSurfaceTexture(pSurfaceTexFile, NULL, (char*)sTextureGloss.c_str(), (char*)sTextureMetalness.c_str());
									WickedCall_ForgetTextureFiles();

									if (pMesh->dwTextureCount < GG_MESH_TEXTURE_SURFACE)
									{
										//PE: Realloc we get a heap errors here.
										//PE: And are generating random crashes from everywhere.
										extern bool EnsureTextureStageValid(sMesh* pMesh, int iTextureStage);
										EnsureTextureStageValid(pMesh, GG_MESH_TEXTURE_SURFACE);

									}

									// and assign this surface to DBO mesh along with channel info (used by importer and other code)
									strcpy ( pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].pName, pSurfaceTexFile);
									pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].channelMask = (0 << 4) + (15);
									strcpy ( pMesh->pTextures[GG_MESH_TEXTURE_ROUGHNESS].pName, pSurfaceTexFile);
									pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].channelMask = (1 << 4) + (15);
									strcpy ( pMesh->pTextures[GG_MESH_TEXTURE_METALNESS].pName, pSurfaceTexFile);
									pMesh->pTextures[GG_MESH_TEXTURE_OCCLUSION].channelMask = (2 << 4) + (15);
								}
								pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name);
								if (pObjectMaterial->textures[MaterialComponent::SURFACEMAP].resource.IsValid())
								{
									pObjectMaterial->roughness = 1;
									pObjectMaterial->metalness = 1;
									pObjectMaterial->SetOcclusionEnabled_Primary(true);
									pObjectMaterial->SetOcclusionEnabled_Secondary(false);
								}
								else
								{
									pObjectMaterial->textures[MaterialComponent::SURFACEMAP].name = "";
								}

								//Try old DNS I
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_i" + sFoundTextureType;
								if (!WickedCall_TextureFileExist((char*)pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name.c_str())) pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureFilenameBase + "_i" + sAltTextureType;
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);
								if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
								{
									pObjectMaterial->SetEmissiveStrength(1.0f);
								}
								else
								{
									pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
								}
							}
						}

						// special flag which moves base color to emissive so surface will be the same
						// no matter which angle it is viewed at or what lights are nearby (smoke, muzzle flashes)
						if (g_iWickedPutInEmissiveMode > 0)
						{
							// emissive override
							if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
							{
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = {};
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
								pObjectMaterial->SetDirty();
								wiJobSystem::context ctx;
								wiJobSystem::Wait(ctx);
							}
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name;
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource;
							if (!pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
							{
								pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
							}
							pObjectMaterial->SetEmissiveStrength(1.0f);
						}

						// ensure material refs updated for rendering
						pObjectMaterial->SetDirty();
					}
				}
			}
		}
	}
}

void WickedCall_TextureObjectAsEmissive(sObject* pObject)
{
	for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
		if (pMesh)
		{
			WickedSetMeshNumber(iMeshIndex);
			WickedCall_SetAsEmissiveMaterial(pMesh);
		}
	}
}

void WickedCall_SetAsEmissiveMaterial(sMesh* pMesh)
{
	if (pMesh)
	{
		// get wicked meshID
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			// get material from mesh
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial)
			{
				// steal the base texture and give it to the emissive
				pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name;
				pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = WickedCall_LoadImage(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name);

				// set base color to black
				pMesh->mMaterial.Diffuse.r = 0;
				pMesh->mMaterial.Diffuse.g = 0;
				pMesh->mMaterial.Diffuse.b = 0;
				pMesh->mMaterial.Diffuse.a = 0;
				pObjectMaterial->SetBaseColor(XMFLOAT4(pMesh->mMaterial.Diffuse.r, pMesh->mMaterial.Diffuse.g, pMesh->mMaterial.Diffuse.b, pMesh->mMaterial.Diffuse.a));

				// set emissve to white
				pMesh->mMaterial.Emissive.r = 1;
				pMesh->mMaterial.Emissive.g = 1;
				pMesh->mMaterial.Emissive.b = 1;
				pMesh->mMaterial.Emissive.a = 1;
				pObjectMaterial->SetEmissiveColor(XMFLOAT4(pMesh->mMaterial.Emissive.r, pMesh->mMaterial.Emissive.g, pMesh->mMaterial.Emissive.b, pMesh->mMaterial.Emissive.a));

				// ensure material refs updated for rendering
				pObjectMaterial->SetDirty();
			}
		}
	}
}

void WickedCall_SetReflectance(sMesh* pMesh, float fReflectance)
{
	if (pMesh)
	{
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			// get material from mesh
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial)
			{
				pObjectMaterial->reflectance = fReflectance;
			}
		}
	}
}

void WickedCall_SetObjectReflectance(sObject* pObject, float fReflectance)
{
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
		if (pObject->ppMeshList[iM])
			WickedCall_SetReflectance(pObject->ppMeshList[iM], fReflectance);
}

float WickedCall_GetObjectReflectance(sObject* pObject)
{
	float fReflectance = 0.0f;
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		sMesh* pMesh = pObject->ppMeshList[iM];
		if (pMesh)
		{
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				uint64_t materialEntity = mesh->subsets[0].materialID;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
				if (pObjectMaterial)
				{
					fReflectance = pObjectMaterial->reflectance;
					break;
				}
			}
		}
	}
	return fReflectance;
}

void WickedCall_SetMeshCullmode(sMesh* pMesh)
{
	if (pMesh)
	{
		// get wicked meshID
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			if(pMesh->iCullMode > 0)
				mesh->SetDoubleSided(false);
			else
				mesh->SetDoubleSided(true);
		}
	}
}

void WickedCall_SetObjectCullmode(sObject* pObject)
{
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
		WickedCall_SetMeshCullmode(pObject->ppMeshList[iM]);
}

void WickedCall_SetObjectDoubleSided(sObject* pObject, bool bDoubleSided)
{
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		sMesh* pMesh = pObject->ppMeshList[iM];
		if (pMesh)
		{
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				mesh->SetDoubleSided(bDoubleSided);
			}
		}
	}
}

bool WickedCall_GetObjectDoubleSided(sObject* pObject)
{
	bool bDoubleSided = false;
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		sMesh* pMesh = pObject->ppMeshList[iM];
		if (pMesh)
		{
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				bDoubleSided = mesh->IsDoubleSided();
				break;
			}
		}
	}
	return bDoubleSided;
}

void WickedCall_SetMeshTransparent(sMesh* pMesh)
{
	if (pMesh)
	{
		// get wicked meshID
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			// get material from mesh
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial)
			{
				// GGMAX 2.09: clearing GG_FORCEDEPTH here is DELIBERATE, and it is what reproduces
				// DX11's semantics. DX11 kept both halves of the state in ONE field (userBlendMode ==
				// BLENDMODE_FORCEDEPTH), so this function simply overwrote it: LAST WRITER WINS. That
				// is exactly why the first-person gun carves and the LUA 3D prompt planes do not —
				// G-Gun_part3.cpp:726-736 calls SetObjectTransparency and then DisableObjectZDepth,
				// while M-LUA-General.cpp:292 calls DisableObjectZDepth and then SetObjectTransparency
				// at :390. Splitting the state into a flag + a blend mode would have let the flag
				// survive this call, and a prompt plane is a single quad whose front faces point AWAY
				// from the camera: it would stamp depth in the prepass, draw nothing in the main
				// front-faces-only pass, and leave an unshaded hole. Keep the two writes together.
				if (pMesh->bTransparency)
					pObjectMaterial->userBlendMode = BLENDMODE_ALPHA;
				else
					pObjectMaterial->userBlendMode = BLENDMODE_OPAQUE;
				pObjectMaterial->SetForceDepth(false);
				pObjectMaterial->SetDirty(); // was IsDirty() — a const getter whose result was discarded
			}
		}
	}
}

void WickedCall_SetObjectTransparent(sObject* pObject)
{
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		//Respect per mesh transparancy.
		if (pObject->ppMeshList[iM])
		{
			WickedSetMeshNumber(iM);
			bool bWickedMaterialActive = IsWickedMaterialActive(pObject->ppMeshList[iM]);
			if (bWickedMaterialActive)
			{
				bool bTransparent = WickedGetTransparent();
				if (bTransparent)
				{
					pObject->ppMeshList[iM]->bTransparency = true;
				}
			}
			WickedCall_SetMeshTransparent(pObject->ppMeshList[iM]);
		}
	}
}

void WickedCall_SetMeshDisableDepth(sMesh* pMesh, bool bDisable)
{
	if (pMesh)
	{
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial)
			{
				// GGMAX 2.09: honour the revert switch HERE too, not just in the renderer. Clearing
				// only the engine bool would leave these materials as plain double-sided transparents
				// — a third state that is neither DX11 nor pre-2.09 DX12, i.e. not a baseline to
				// compare against. Reading it here makes `setup.ini weaponforcedepth=0` a true revert.
				// (The harness `SET_WEAPONDEPTH` flips only the engine half, since materials are set up
				// once at gun load; it is an A/B lever, not a revert. Documented as such.)
				if (bDisable == true && wi::renderer::gg_weapon_forcedepth)
				{
					// GGMAX 2.09: restores the DX11 fork's BLENDMODE_FORCEDEPTH behaviour.
					//
					// DX11 set userBlendMode = BLENDMODE_FORCEDEPTH here, which did two things at once:
					// it put the material in the TRANSPARENT pass (any non-OPAQUE blend mode does), and
					// it selected a pipeline that stamps the mesh's own depth with compare ALWAYS so the
					// first-person weapon can never be clipped by the wall the player is standing in.
					// The DX12 port had no FORCEDEPTH blend mode to map to and left this at OPAQUE, which
					// silently dropped the carve — measured: a crate cut the front half off the pistol.
					//
					// Split into the two things it meant: ALPHA for the transparent pass, and the
					// GG_FORCEDEPTH material flag for the carve (engine 2.09 reads it in RenderMeshes).
					// SHADERTYPE_WEAPON is still not ported — that was the separate WEAPON_SHADOW
					// lighting hack, which needs a new object-shader permutation.
					pObjectMaterial->userBlendMode = BLENDMODE_ALPHA; // was BLENDMODE_FORCEDEPTH
					pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR; // was SHADERTYPE_WEAPON (NOT PORTED)
					pObjectMaterial->SetForceDepth(true);
					pObjectMaterial->SetDoubleSided(true);
				}
				else
				{
					pObjectMaterial->userBlendMode = BLENDMODE_OPAQUE;
					pObjectMaterial->SetForceDepth(false);
					pObjectMaterial->SetDoubleSided(false);
				}
				pObjectMaterial->SetDirty(); // was IsDirty() — a const getter whose result was discarded
			}
		}
	}
}

void WickedCall_SetObjectDisableDepth(sObject* pObject, bool bDisable)
{
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		if (pObject->ppMeshList[iM])
		{
			WickedCall_SetMeshDisableDepth(pObject->ppMeshList[iM], bDisable);
		}
	}
	if (bDisable == false)
	{
		// additionally restore if object transparent or opaque, and doublesided(cull mode)
		WickedCall_SetObjectTransparent(pObject);
		WickedCall_SetObjectCullmode(pObject);
	}
}

std::string WickedCall_GetAllTexturesUsed(sObject* pObject)
{
	std::string sTmp = "";
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		sMesh* pMesh = pObject->ppMeshList[iM];
		if (pMesh)
		{
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				// get material from mesh
				uint64_t materialEntity = mesh->subsets[0].materialID;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
				if (pObjectMaterial)
				{
					for (int i = 0; i < MaterialComponent::TEXTURESLOT_COUNT; i++)
					{
						//Extract textures.
						if (pObjectMaterial->textures[i].resource.IsValid())
						{
							if (pObjectMaterial->textures[i].name.length() > 0)
							{
								const char *pestrcasestr(const char *arg1, const char *arg2);
								if( !pestrcasestr(sTmp.c_str(), pObjectMaterial->textures[i].name.c_str()) )
									sTmp += pObjectMaterial->textures[i].name + "|";
							}
						}
					}
				}
			}
		}
	}
	return sTmp;
}

void WickedCall_SetMeshAlpha(sMesh* pMesh, float fPercentage)
{
	if (pMesh)
	{
		// get wicked meshID
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			// get material from mesh
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial)
			{
				pObjectMaterial->SetOpacity(fPercentage/100.0);
				pObjectMaterial->IsDirty();
			}
		}
	}
}

LPSTR WickedCall_GetMeshMaterialName(sMesh* pMesh)
{
	LPSTR pName = NULL;
	if (pMesh)
	{
		// get wicked meshID
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			// get material from mesh
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial)
			{
				// return a pointer to the material basecolor name (used to determine if a successful texture was loaded)
				// in place of the old method of checking the iImageID which now may be zero
				if ( pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource.IsValid() )
					pName = (LPSTR)pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name.c_str();
			}
		}
	}
	return pName;
}

void WickedCall_GetFrameWorldPos(sFrame* pFrame, float* pfX, float* pfY, float* pfZ)
{
	if (pFrame)
	{
		uint64_t wickedobjindex = pFrame->wickedobjindex;
		wiScene::TransformComponent* pFrameTransform = wiScene::GetScene().transforms.GetComponent(wickedobjindex);
		if (pFrameTransform)
		{
			*pfX = pFrameTransform->GetPosition().x;
			*pfY = pFrameTransform->GetPosition().y;
			*pfZ = pFrameTransform->GetPosition().z;
		}
	}
}

void WickedCall_GetGluedLimbWorldPos(sObject* pObject, int iLimbID, float* pfX, float* pfY, float* pfZ)
{
	int iObjectParent = pObject->position.iGluedToObj;
	sObject* GetObjectData(int iID);
	sObject* pGluedTo = GetObjectData(iObjectParent);
	if (pGluedTo)
	{
		sFrame* pFrame = pObject->ppFrameList[0];
		if (iLimbID < pObject->iFrameCount) pFrame = pObject->ppFrameList[iLimbID];
		uint64_t wickedobjindex = pFrame->wickedobjindex;
		wiScene::TransformComponent* pFrameTransform = wiScene::GetScene().transforms.GetComponent(wickedobjindex);
		if (pFrameTransform)
		{
			wiScene::TransformComponent transform;
			transform.ClearTransform();
			transform.Translate(XMFLOAT3(0, 0, 0));
			transform.RotateRollPitchYaw(XMFLOAT3(0, 0, 0));
			transform.UpdateTransform_Parented(*pFrameTransform);
			*pfX = transform.GetPosition().x;
			*pfY = transform.GetPosition().y;
			*pfZ = transform.GetPosition().z;
		}
	}
}

void WickedCall_GetLimbDataEx(sObject* pObject, int iLimbID, bool bAdjustLimb, float fX, float fY, float fZ, float fAX, float fAY, float fAZ, float* pX, float* pY, float* pZ, float* pQAX, float* pQAY, float* pQAZ, float* pQAW)
{
	if ( pObject )
	{
		sFrame* pFrame = pObject->ppFrameList[iLimbID];
		if ( pFrame )
		{
			uint64_t iFrameWickedObjectNumber = pFrame->wickedobjindex;
			if (iFrameWickedObjectNumber > 0)
			{
				// set a transform for the object
				wiScene::TransformComponent* pTransform = wiScene::GetScene().transforms.GetComponent(iFrameWickedObjectNumber);
				if (pObject->position.bCustomWorldMatrix == false)
				{
					if (bAdjustLimb)
					{
						wiScene::TransformComponent transform;
						transform.ClearTransform();
						transform.Translate(XMFLOAT3(fX, fY, fZ));
						transform.RotateRollPitchYaw(XMFLOAT3(fAX, fAY, fAZ));
						//PE: Optimizing this is hitting TransformComponent::GetLocalMatrix() heavy.
						transform.UpdateTransform_Parented(*pTransform);
						*pX = transform.GetPosition().x;
						*pY = transform.GetPosition().y;
						*pZ = transform.GetPosition().z;
						if (pQAX)
						{
							*pQAX = transform.GetRotation().x;
							*pQAY = transform.GetRotation().y;
							*pQAZ = transform.GetRotation().z;
							*pQAW = transform.GetRotation().w;
						}
					}
					else
					{
						*pX = pTransform->GetPosition().x;
						*pY = pTransform->GetPosition().y;
						*pZ = pTransform->GetPosition().z;
						if (pQAX)
						{
							*pQAX = pTransform->GetRotation().x;
							*pQAY = pTransform->GetRotation().y;
							*pQAZ = pTransform->GetRotation().z;
							*pQAW = pTransform->GetRotation().w;
						}
					}
				}
			}
		}
	}
}

void WickedCall_GetLimbLocalPosAndRot(sObject* pObject, int iLimbID, float* pX, float* pY, float* pZ, float* pQAX, float* pQAY, float* pQAZ, float* pQAW)
{
	if (pObject)
	{
		sFrame* pFrame = pObject->ppFrameList[iLimbID];
		if (pFrame)
		{
			uint64_t iFrameWickedObjectNumber = pFrame->wickedobjindex;
			if (iFrameWickedObjectNumber > 0)
			{
				// set a transform for the object
				wiScene::TransformComponent* pTransform = wiScene::GetScene().transforms.GetComponent(iFrameWickedObjectNumber);
				*pX = pTransform->translation_local.x;
				*pY = pTransform->translation_local.y;
				*pZ = pTransform->translation_local.z;
				*pQAX = pTransform->rotation_local.x;
				*pQAY = pTransform->rotation_local.y;
				*pQAZ = pTransform->rotation_local.z;
				*pQAW = pTransform->rotation_local.w;
			}
		}
	}
}

// GG: a limb's local position, rotation and scale (10 floats) and its world matrix (16 floats, row by row) as last
// updated; a limb with no transform gets no turn or move
void WickedCall_GetLimbLocalAndWorld(sObject* pObject, int iLimbID, float* pLocal, float* pWorld)
{
	const float fNone[10] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 };
	for (int i = 0; i < 10; i++) pLocal[i] = fNone[i];
	XMStoreFloat4x4((XMFLOAT4X4*)pWorld, XMMatrixIdentity());
	if (!pObject || iLimbID < 0 || iLimbID >= pObject->iFrameCount) return;
	sFrame* pFrame = pObject->ppFrameList[iLimbID];
	if (!pFrame || pFrame->wickedobjindex == 0) return;
	TransformComponent* pTransform = wiScene::GetScene().transforms.GetComponent(pFrame->wickedobjindex);
	if (!pTransform) return;
	pLocal[0] = pTransform->translation_local.x;
	pLocal[1] = pTransform->translation_local.y;
	pLocal[2] = pTransform->translation_local.z;
	pLocal[3] = pTransform->rotation_local.x;
	pLocal[4] = pTransform->rotation_local.y;
	pLocal[5] = pTransform->rotation_local.z;
	pLocal[6] = pTransform->rotation_local.w;
	pLocal[7] = pTransform->scale_local.x;
	pLocal[8] = pTransform->scale_local.y;
	pLocal[9] = pTransform->scale_local.z;
	*(XMFLOAT4X4*)pWorld = pTransform->world;
}

// GG: every limb's world matrix (16 floats each, row by row) with the object's animation at a frame, worked out apart
// from the scene, which it leaves as it is. A limb the animation moves takes its channels' values at that frame (as
// the animation update samples them, its fixed frames kept), the rest their locals as given (pMadeLocals, 10 floats
// each); a limb whose parent is no limb hangs from where that parent was when pMadeLocals and pMadeWorlds were taken.
// False when the object has no animation
bool WickedCall_GetLimbWorldsAtFrame(sObject* pObject, float fFrame, const float* pMadeLocals, const float* pMadeWorlds, float* pWorlds)
{
	if (!pObject || !pObject->pAnimationSet || pObject->iFrameCount <= 0) return false;
	Scene& scene = wiScene::GetScene();
	AnimationComponent* pAnim = scene.animations.GetComponent(pObject->pAnimationSet->wickedanimentityindex);
	if (!pAnim) return false;
	const int iLimbs = pObject->iFrameCount;
	std::vector<XMFLOAT3> translation(iLimbs), scale(iLimbs);
	std::vector<XMFLOAT4> rotation(iLimbs);
	std::unordered_map<Entity, int> limbOf;
	for (int i = 0; i < iLimbs; i++)
	{
		const float* pLocal = pMadeLocals + (i * 10);
		translation[i] = XMFLOAT3(pLocal[0], pLocal[1], pLocal[2]);
		rotation[i] = XMFLOAT4(pLocal[3], pLocal[4], pLocal[5], pLocal[6]);
		scale[i] = XMFLOAT3(pLocal[7], pLocal[8], pLocal[9]);
		sFrame* pFrame = pObject->ppFrameList[i];
		if (pFrame && pFrame->wickedobjindex > 0) limbOf[pFrame->wickedobjindex] = i;
	}

	// each channel's value at the frame, from the keys either side of it (a cubic spline's values without its tangents)
	for (const AnimationComponent::AnimationChannel& channel : pAnim->channels)
	{
		if (channel.path == AnimationComponent::AnimationChannel::Path::WEIGHTS) continue;
		std::unordered_map<Entity, int>::iterator it = limbOf.find(channel.target);
		if (it == limbOf.end()) continue;
		if (channel.samplerIndex < 0 || channel.samplerIndex >= (int)pAnim->samplers.size()) continue;
		const AnimationComponent::AnimationSampler& sampler = pAnim->samplers[channel.samplerIndex];
		const AnimationDataComponent* pData = scene.animation_datas.GetComponent(sampler.data);
		if (!pData || pData->keyframe_times.empty()) continue;
		const int iKeys = (int)pData->keyframe_times.size();
		int iRight = 0;
		while (iRight < iKeys && pData->keyframe_times[iRight] < fFrame) iRight++;
		if (iRight >= iKeys) iRight = iKeys - 1;
		int iLeft = std::max(0, iRight - 1);
		if (channel.iUsePreFrame >= 10000 && channel.path == AnimationComponent::AnimationChannel::Path::ROTATION)
		{
			iLeft = std::min(channel.iUsePreFrame - 10000, iKeys - 1);
			iRight = iLeft;
		}
		float fT = 0;
		if (iLeft != iRight && sampler.mode != AnimationComponent::AnimationSampler::Mode::STEP)
		{
			float fSpan = pData->keyframe_times[iRight] - pData->keyframe_times[iLeft];
			if (fSpan > 0) fT = std::min(std::max((fFrame - pData->keyframe_times[iLeft]) / fSpan, 0.0f), 1.0f);
		}
		const bool bCubic = (sampler.mode == AnimationComponent::AnimationSampler::Mode::CUBICSPLINE);
		const int iStride = bCubic ? 3 : 1;
		const int iOffset = bCubic ? 1 : 0;
		const int iWidth = (channel.path == AnimationComponent::AnimationChannel::Path::ROTATION) ? 4 : 3;
		if (pData->keyframe_data.size() < (size_t)(((iRight * iStride) + iOffset + 1) * iWidth)) continue;
		const float* pLeft = pData->keyframe_data.data() + (((iLeft * iStride) + iOffset) * iWidth);
		const float* pRight = pData->keyframe_data.data() + (((iRight * iStride) + iOffset) * iWidth);
		int iLimb = it->second;
		if (channel.path == AnimationComponent::AnimationChannel::Path::ROTATION)
		{
			XMVECTOR vRotation = XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4((const XMFLOAT4*)pLeft), XMLoadFloat4((const XMFLOAT4*)pRight), fT));
			XMStoreFloat4(&rotation[iLimb], vRotation);
		}
		else
		{
			XMVECTOR vValue = XMVectorLerp(XMLoadFloat3((const XMFLOAT3*)pLeft), XMLoadFloat3((const XMFLOAT3*)pRight), fT);
			if (channel.path == AnimationComponent::AnimationChannel::Path::TRANSLATION) XMStoreFloat3(&translation[iLimb], vValue);
			else XMStoreFloat3(&scale[iLimb], vValue);
		}
	}

	// each limb's world from its parent's, as the hierarchy update makes it, parents first
	std::vector<char> done(iLimbs, 0);
	for (int iPass = 0; iPass < iLimbs; iPass++)
	{
		bool bAny = false;
		for (int i = 0; i < iLimbs; i++)
		{
			if (done[i]) continue;
			sFrame* pFrame = pObject->ppFrameList[i];
			XMMATRIX world;
			if (!pFrame || pFrame->wickedobjindex == 0)
			{
				world = XMLoadFloat4x4((const XMFLOAT4X4*)(pMadeWorlds + (i * 16)));
			}
			else
			{
				int iParent = -1;
				HierarchyComponent* pHierarchy = scene.hierarchy.GetComponent(pFrame->wickedobjindex);
				if (pHierarchy)
				{
					std::unordered_map<Entity, int>::iterator itParent = limbOf.find(pHierarchy->parentID);
					if (itParent != limbOf.end()) iParent = itParent->second;
				}
				if (iParent >= 0 && !done[iParent]) continue;
				XMMATRIX local = XMMatrixScalingFromVector(XMLoadFloat3(&scale[i])) * XMMatrixRotationQuaternion(XMLoadFloat4(&rotation[i])) * XMMatrixTranslationFromVector(XMLoadFloat3(&translation[i]));
				XMMATRIX parentWorld;
				if (iParent >= 0)
				{
					parentWorld = XMLoadFloat4x4((const XMFLOAT4X4*)(pWorlds + (iParent * 16)));
				}
				else
				{
					const float* pLocal = pMadeLocals + (i * 10);
					XMMATRIX madeLocal = XMMatrixScaling(pLocal[7], pLocal[8], pLocal[9]) * XMMatrixRotationQuaternion(XMVectorSet(pLocal[3], pLocal[4], pLocal[5], pLocal[6])) * XMMatrixTranslation(pLocal[0], pLocal[1], pLocal[2]);
					parentWorld = XMMatrixInverse(nullptr, madeLocal) * XMLoadFloat4x4((const XMFLOAT4X4*)(pMadeWorlds + (i * 16)));
				}
				world = local * parentWorld;
			}
			XMStoreFloat4x4((XMFLOAT4X4*)(pWorlds + (i * 16)), world);
			done[i] = 1;
			bAny = true;
		}
		if (!bAny) break;
	}
	for (int i = 0; i < iLimbs; i++)
		if (!done[i]) memcpy(pWorlds + (i * 16), pMadeWorlds + (i * 16), sizeof(float) * 16);
	return true;
}

void WickedCall_GetLimbData(sObject* pObject, int iLimbID, float* pX, float* pY, float* pZ, float* pQAX, float* pQAY, float* pQAZ, float* pQAW)
{
	WickedCall_GetLimbDataEx(pObject, iLimbID, false, 0, 0, 0, 0, 0, 0, pX, pY, pZ, pQAX, pQAY, pQAZ, pQAW);
}

void WickedCall_UpdateMeshVertexData(sMesh* pDBOMesh)
{
	if (pDBOMesh)
	{
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pDBOMesh->wickedmeshindex);
		if (mesh)
		{
			// for now, we are only interested in updating the UV data from the original to the wicked mesh
			sOffsetMap offsetMap;
			GetFVFOffsetMapFixedForBones(pDBOMesh, &offsetMap);

			//PE: The size can change in phyics debug drawer so:
			if(offsetMap.dwZ > 0 && mesh->vertex_positions.size() != pDBOMesh->dwVertexCount)
				mesh->vertex_positions.resize(pDBOMesh->dwVertexCount);
			if (offsetMap.dwTU[0] > 0 && mesh->vertex_uvset_0.size() != pDBOMesh->dwVertexCount)
				mesh->vertex_uvset_0.resize(pDBOMesh->dwVertexCount);
			if (offsetMap.dwNZ > 0 && mesh->vertex_normals.size() != pDBOMesh->dwVertexCount)
				mesh->vertex_normals.resize(pDBOMesh->dwVertexCount);

			for (size_t v = 0; v < pDBOMesh->dwVertexCount; v++)
			{
				if (offsetMap.dwZ > 0)
				{
					XMFLOAT3 pos = XMFLOAT3(0, 0, 0);
					pos.x = *(float*)((float*)pDBOMesh->pVertexData + offsetMap.dwX + (offsetMap.dwSize * v));
					pos.y = *(float*)((float*)pDBOMesh->pVertexData + offsetMap.dwY + (offsetMap.dwSize * v));
					pos.z = *(float*)((float*)pDBOMesh->pVertexData + offsetMap.dwZ + (offsetMap.dwSize * v));
					mesh->vertex_positions[v] = pos;
				}
				if (offsetMap.dwTU[0] > 0)
				{
					XMFLOAT2 tex = XMFLOAT2(0, 0);
					tex.x = *(float*)((float*)pDBOMesh->pVertexData + offsetMap.dwTU[0] + (offsetMap.dwSize * v));
					tex.y = *(float*)((float*)pDBOMesh->pVertexData + offsetMap.dwTV[0] + (offsetMap.dwSize * v));
					mesh->vertex_uvset_0[v] = tex;
				}
			}
			mesh->CreateRenderData();
		}
	}
}

void WickedCall_SetObjectAlpha(sObject* pObject, float fPercentage)
{
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		WickedSetMeshNumber(iM);
		WickedCall_SetMeshAlpha(pObject->ppMeshList[iM], fPercentage);
	}
}

float WickedCall_GetObjectAlpha(sObject* pObject)
{
	float fPercentage = 1.0f;
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		sMesh* pMesh = pObject->ppMeshList[iM];
		if (pMesh)
		{
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				uint64_t materialEntity = mesh->subsets[0].materialID;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
				if (pObjectMaterial)
				{
					fPercentage = pObjectMaterial->GetOpacity() * 100.0f;
					break;
				}
			}
		}
	}
	return fPercentage;
}

void WickedCall_SetObjectTransparentDirect(sObject* pObject, bool bTransparent)
{
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		pObject->ppMeshList[iM]->bTransparency = bTransparent;
		WickedCall_SetMeshTransparent(pObject->ppMeshList[iM]);
	}
}
bool WickedCall_GetObjectTransparentDirect(sObject* pObject)
{
	bool bTransparent = false;
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		bTransparent = pObject->ppMeshList[iM]->bTransparency;
		break;
	}
	return bTransparent;
}

void WickedCall_SetObjectBlendMode(sObject* pObject, int iBlendmode)
{
	if (!pObject) return;

	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		if (pObject->ppMeshList[iM])
		{
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pObject->ppMeshList[iM]->wickedmeshindex);
			if (mesh)
			{
				uint64_t materialEntity = mesh->subsets[0].materialID;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
				if (pObjectMaterial)
				{
					pObjectMaterial->userBlendMode = (BLENDMODE) iBlendmode;
					pObjectMaterial->SetDirty(true);
				}
			}
		}
	}
}


void WickedCall_SetObjectAlphaRef(sObject* pObject, float fAlphaRef)
{
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
		WickedCall_SetMeshAlphaRef(pObject->ppMeshList[iM], fAlphaRef);
}

float WickedCall_GetObjectAlphaRef(sObject* pObject)
{
	float fAlphaRef = 1.0f;
	for (int iM = 0; iM < pObject->iMeshCount; iM++)
	{
		sMesh* pMesh = pObject->ppMeshList[iM];
		if (pMesh)
		{
			// get wicked meshID
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				// get material from mesh
				uint64_t materialEntity = mesh->subsets[0].materialID;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
				if (pObjectMaterial)
				{
					fAlphaRef = pObjectMaterial->alphaRef;// GetAlphaRef();
					break;
				}
			}
		}
	}
	return fAlphaRef;
}

void WickedCall_SetMeshAlphaRef(sMesh* pMesh, float fAlphaRef)
{
	if (pMesh)
	{
		// get wicked meshID
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			// get material from mesh
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial)
			{
				pObjectMaterial->SetAlphaRef(fAlphaRef);
			}
		}
	}
}

void WickedCall_SetMeshMaterial ( sMesh* pMesh, bool bForce)
{
	if (pMesh)
	{
		// get wicked meshID
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			// get material from mesh
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial)
			{
				// base color
				bool bWickedMaterialActive = IsWickedMaterialActive(pMesh);
				if (!bForce && bWickedMaterialActive)
				{
					DWORD dwBaseColor = WickedGetBaseColor();
					if (dwBaseColor != -1)
					{
						pMesh->mMaterial.Diffuse.r = ((dwBaseColor & 0xff000000) >> 24) / 255.0f;;
						pMesh->mMaterial.Diffuse.g = ((dwBaseColor & 0x00ff0000) >> 16) / 255.0f;
						pMesh->mMaterial.Diffuse.b = ((dwBaseColor & 0x0000ff00) >> 8) / 255.0f;
						pMesh->mMaterial.Diffuse.a = (dwBaseColor & 0x000000ff) / 255.0f;
					}
					pObjectMaterial->SetBaseColor(XMFLOAT4(pMesh->mMaterial.Diffuse.r, pMesh->mMaterial.Diffuse.g, pMesh->mMaterial.Diffuse.b, pMesh->mMaterial.Diffuse.a));
				}
				else
				{
					pObjectMaterial->SetBaseColor(XMFLOAT4(pMesh->mMaterial.Diffuse.r, pMesh->mMaterial.Diffuse.g, pMesh->mMaterial.Diffuse.b, pMesh->mMaterial.Diffuse.a));
				}

				// emissive color
				//PE: Prevent us for setting emissive color to black ? another way is needed.
				//PE: Only do trick if we dont have any custom material settings.
				if (!bWickedMaterialActive && pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid() && (pMesh->mMaterial.Emissive.r + pMesh->mMaterial.Emissive.g + pMesh->mMaterial.Emissive.b) == 0)
				{
					// a trick so that old mesh materials set to black can be seen in new wicked engine
					pObjectMaterial->emissiveColor.x = 1.0f;
					pObjectMaterial->emissiveColor.y = 1.0f;
					pObjectMaterial->emissiveColor.z = 1.0f;
				}
				else 
				{
					// otherwise normally have values set in pMesh->mMaterial
					pObjectMaterial->emissiveColor.x = pMesh->mMaterial.Emissive.r;
					pObjectMaterial->emissiveColor.y = pMesh->mMaterial.Emissive.g;
					pObjectMaterial->emissiveColor.z = pMesh->mMaterial.Emissive.b;
					if (!pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
					{
						//PE: emissiveColor.w is used for emissive strength , so dont touch if we have a texture :)
						pObjectMaterial->emissiveColor.w = pMesh->mMaterial.Emissive.a;
					}
				}

				// ensure material refs updated for rendering
				pObjectMaterial->SetDirty();
			}
		}
	}
}

void WickedCall_SetObjectRenderOrderBias(sObject* pObject, float fRenderOrderBias)
{
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				// get wicked meshID
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					WickedCall_SetRenderOrderBias(pMesh, fRenderOrderBias);
				}
			}
		}
	}
}

float WickedCall_GetObjectRenderOrderBias(sObject* pObject)
{
	float fRenderOrderBias = 0.0f;
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		sFrame* pFrame = pObject->ppFrameList[i];
		if(pFrame)
		{
			ObjectComponent* object = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
			if (object)
			{
				fRenderOrderBias = (float)object->sort_priority;
				break;
			}
		}
	}
	return fRenderOrderBias;
}

void WickedCall_SetObjectPlanerReflection(sObject* pObject, bool bPlanerReflection)
{
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				// get wicked meshID
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					// get material from mesh
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						if (bPlanerReflection)
						{
							pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR_PLANARREFLECTION;
						}
						else
						{
							if(pObjectMaterial->parallaxOcclusionMapping > 0.0f )
								pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING;
							else
								pObjectMaterial->shaderType = MaterialComponent::SHADERTYPE_PBR;
						}
					}
				}
			}
		}
	}
}

bool WickedCall_GetObjectPlanerReflection(sObject* pObject)
{
	bool bPlanerReflection = false;
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				// get wicked meshID
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					// get material from mesh
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						if (pObjectMaterial->shaderType == MaterialComponent::SHADERTYPE_PBR_PLANARREFLECTION)
						{
							bPlanerReflection = true;
							break;
						}
					}
				}
			}
		}
	}
	return bPlanerReflection;
}

