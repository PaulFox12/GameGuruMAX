#include "GGAnimBridge.h"

bool bActivateStandaloneOutline = false;

void WickedCall_SetObjectOutline(sObject* pObject, float fHighlight)
{
	extern std::vector<int> g_StandaloneObjectHighlightList;
	if (pObject)
	{
		if (ObjectExist(pObject->dwObjectNumber))
		{
			if (fHighlight > 1.1)
			{
				//PE: Keep a list and keep updating them.
				g_StandaloneObjectHighlightList.push_back(pObject->dwObjectNumber);
				bActivateStandaloneOutline = true;
			}
			else if (fHighlight > 0.1)
			{
				//PE: Fire only one time. effect will be lost on next frame.
				WickedCall_DrawObjctBox(pObject, XMFLOAT4(0.8f, 0.8f, 0.8f, 0.8f), false, false);
				bActivateStandaloneOutline = true;
			}
			else
			{
				for (int i = 0; i < g_StandaloneObjectHighlightList.size(); i++)
				{
					if (g_StandaloneObjectHighlightList[i] == pObject->dwObjectNumber)
					{
						g_StandaloneObjectHighlightList.erase(g_StandaloneObjectHighlightList.begin() + i);
						break;
					}
				}
			}
		}
	}
}

bool WickedCall_GetObjectOutline(sObject* pObject)
{
	extern std::vector<int> g_StandaloneObjectHighlightList;
	if (pObject)
	{
		if (ObjectExist(pObject->dwObjectNumber))
		{
			for (int i = 0; i < g_StandaloneObjectHighlightList.size(); i++)
			{
				if (g_StandaloneObjectHighlightList[i] == pObject->dwObjectNumber)
				{
					g_StandaloneObjectHighlightList.erase(g_StandaloneObjectHighlightList.begin() + i);
					return(true);
				}
			}
		}
	}
	return(false);
}

void WickedCall_SetObjectCastShadows(sObject* pObject, bool bCastShadow)
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
						if (bCastShadow)
							pObjectMaterial->SetCastShadow(true);
						else
							pObjectMaterial->SetCastShadow(false);
					}
				}
			}
		}
	}
}

bool WickedCall_GetObjectCastShadows(sObject* pObject)
{
	bool bCastShadow = true;
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
						bCastShadow = pObjectMaterial->IsCastingShadow();// > IsCastingShadow;// IsCastShadow();
						break;
					}
				}
			}
		}
	}
	return bCastShadow;
}

void WickedCall_SetObjectTextureUV(sObject* pObject, float x, float y, float z, float w)
{
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						pObjectMaterial->texMulAdd.x = x;
						pObjectMaterial->texMulAdd.y = y;
						pObjectMaterial->texMulAdd.z = z;
						pObjectMaterial->texMulAdd.w = w;
						pObjectMaterial->SetDirty(true);
					}
				}
			}
		}
	}
}

void WickedCall_GetObjectTextureUV(sObject* pObject, float* x, float* y, float* z, float* w)
{
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						*x = pObjectMaterial->texMulAdd.x;
						*y = pObjectMaterial->texMulAdd.y;
						*z = pObjectMaterial->texMulAdd.z;
						*w = pObjectMaterial->texMulAdd.w;
					}
				}
			}
		}
	}
}

void WickedCall_SetObjectLightToUnlit(sObject* pObject, int shaderType)
{
	for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMesh];
		if (pMesh)
		{
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				uint64_t materialEntity = mesh->subsets[0].materialID;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
				if (pObjectMaterial)
				{
					pObjectMaterial->SetReflectance(0.0f);
					pObjectMaterial->shaderType = (wiScene::MaterialComponent::SHADERTYPE)shaderType;
					pObjectMaterial->SetDirty(true);
				}
			}
			WickedCall_SetMeshMaterial(pMesh, false);
		}
	}
}

void WickedCall_SetObjectBaseColor(sObject* pObject, int r, int g, int b)
{
	for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMesh];
		if (pMesh)
		{
			pMesh->mMaterial.Diffuse.r = r / 255.0f;
			pMesh->mMaterial.Diffuse.g = g / 255.0f;
			pMesh->mMaterial.Diffuse.b = b / 255.0f;
			WickedCall_SetMeshMaterial(pMesh,true);
		}
	}
}

void WickedCall_GetObjectBaseColor(sObject* pObject, int* r, int* g, int* b)
{
	for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMesh];
		if (pMesh)
		{
			*r = pMesh->mMaterial.Diffuse.r * 255.0f;
			*g = pMesh->mMaterial.Diffuse.g * 255.0f;
			*b = pMesh->mMaterial.Diffuse.b * 255.0f;
			break;
		}
	}
}

// each mesh's own copy of its base colour made the same as the material it renders with (ResetEntityMaterial); instanced
// meshes, which use their master object's materials, are left alone
void WickedCall_CopyMaterialColorsToMeshes(sObject* pObject)
{
	if (!pObject || !pObject->ppMeshList) return;
	for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMesh];
		if (!pMesh || (pMesh->bInstanced && pMesh->wickedmaterialindex == 0 && pMesh->master_wickedmaterialindex > 0)) continue;
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (!mesh || mesh->subsets.empty()) continue;
		wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(mesh->subsets[0].materialID);
		if (!pObjectMaterial) continue;
		pMesh->mMaterial.Diffuse.r = pObjectMaterial->baseColor.x;
		pMesh->mMaterial.Diffuse.g = pObjectMaterial->baseColor.y;
		pMesh->mMaterial.Diffuse.b = pObjectMaterial->baseColor.z;
		pMesh->mMaterial.Diffuse.a = pObjectMaterial->baseColor.w;
	}
}

// one mesh's (material slot's) base or emissive colour, 0 to 255, as the object-wide setters and getters do for them all
void WickedCall_SetMeshBaseColor(sMesh* pMesh, int r, int g, int b)
{
	if (!pMesh) return;
	pMesh->mMaterial.Diffuse.r = r / 255.0f;
	pMesh->mMaterial.Diffuse.g = g / 255.0f;
	pMesh->mMaterial.Diffuse.b = b / 255.0f;
	WickedCall_SetMeshMaterial(pMesh, true);
}

void WickedCall_GetMeshBaseColor(sMesh* pMesh, int* r, int* g, int* b)
{
	if (!pMesh) return;
	*r = pMesh->mMaterial.Diffuse.r * 255.0f;
	*g = pMesh->mMaterial.Diffuse.g * 255.0f;
	*b = pMesh->mMaterial.Diffuse.b * 255.0f;
}

void WickedCall_SetMeshEmissiveColor(sMesh* pMesh, int r, int g, int b)
{
	if (!pMesh) return;
	pMesh->mMaterial.Emissive.r = r / 255.0f;
	pMesh->mMaterial.Emissive.g = g / 255.0f;
	pMesh->mMaterial.Emissive.b = b / 255.0f;
	WickedCall_SetMeshMaterial(pMesh, false);
}

void WickedCall_GetMeshEmissiveColor(sMesh* pMesh, int* r, int* g, int* b)
{
	if (!pMesh) return;
	*r = pMesh->mMaterial.Emissive.r * 255.0f;
	*g = pMesh->mMaterial.Emissive.g * 255.0f;
	*b = pMesh->mMaterial.Emissive.b * 255.0f;
}

void WickedCall_SetObjectEmissiveColor(sObject* pObject, int r, int g, int b)
{
	for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMesh];
		if (pMesh)
		{
			pMesh->mMaterial.Emissive.r = r / 255.0f;
			pMesh->mMaterial.Emissive.g = g / 255.0f;
			pMesh->mMaterial.Emissive.b = b / 255.0f;
			WickedCall_SetMeshMaterial(pMesh, false);
		}
	}
}

void WickedCall_GetObjectEmissiveColor(sObject* pObject, int* r, int* g, int* b)
{
	for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMesh];
		if (pMesh)
		{
			*r = pMesh->mMaterial.Emissive.r * 255.0f;
			*g = pMesh->mMaterial.Emissive.g * 255.0f;
			*b = pMesh->mMaterial.Emissive.b * 255.0f;
			break;
		}
	}
}

void WickedCall_SetObjectNormalness(sObject* pObject, float normalness)
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
						pObjectMaterial->SetNormalMapStrength(normalness);
					}
				}
			}
		}
	}
}
float WickedCall_GetObjectNormalness(sObject* pObject)
{
	float normalness = 1.0f;
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
						normalness = pObjectMaterial->normalMapStrength;// GetNormalMapStrength();
						break;
					}
				}
			}
		}
	}
	return normalness;
}
void WickedCall_SetObjectRoughness(sObject* pObject, float roughness)
{
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						pObjectMaterial->SetRoughness(roughness);
					}
				}
			}
		}
	}
}
float WickedCall_GetObjectRoughness(sObject* pObject)
{
	float roughness = 0.0f;
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						roughness = pObjectMaterial->roughness;// ->GetRoughness();
						break;
					}
				}
			}
		}
	}
	return roughness;
}
void WickedCall_SetObjectMetalness(sObject* pObject, float metalness)
{
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						pObjectMaterial->SetMetalness(metalness);
					}
				}
			}
		}
	}
}
float WickedCall_GetObjectMetalness(sObject* pObject)
{
	float metalness = 0.0f;
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						metalness = pObjectMaterial->metalness;// GetMetalness();
					}
				}
			}
		}
	}
	return metalness;
}

void WickedCall_SetObjectEmissiveStrength(sObject* pObject, float strength)
{
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						pObjectMaterial->SetEmissiveStrength(strength);
					}
				}
			}
		}
	}
}

float WickedCall_GetObjectEmissiveStrength(sObject* pObject)
{
	float strength = 0.0f;
	for (int i = 0; i < pObject->iFrameCount; i++)
	{
		if (pObject->ppFrameList[i]->pMesh)
		{
			sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
			if (pMesh)
			{
				wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
				if (mesh)
				{
					uint64_t materialEntity = mesh->subsets[0].materialID;
					wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
					if (pObjectMaterial)
					{
						strength = pObjectMaterial->GetEmissiveStrength();
						break;
					}
				}
			}
		}
	}
	return strength;
}

void WickedCall_TextureObject(sObject* pObject,sMesh* pJustThisMesh)
{
	for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
		if (pMesh)
		{
			if (pJustThisMesh == NULL || (pJustThisMesh == pMesh)) 
			{
				WickedSetMeshNumber(iMeshIndex);
				WickedCall_TextureMesh(pMesh);
			}
		}
	}
}

void WickedCall_TextureMeshWithImagePtr(sMesh* pMesh, int iPutInEmissivemode)
{
	//PE: This system can only be used for prompt and other FORMAT_R8G8B8A8_UNORM textures.
	//PE: If you try it with anything else it will crash.
	if (pMesh)
	{
		// get wicked meshID
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
		if (mesh)
		{
			// get material from mesh
			uint64_t materialEntity = mesh->subsets[0].materialID;
			wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
			if (pObjectMaterial && pMesh->pTextures)
			{
				// get old DX11 image ptr
				sTexture* pMeshTexture = &pMesh->pTextures[0];
				if (pMeshTexture)
				{
					ID3D11Resource* pTexturePtr = pMeshTexture->pTexturesRef;
					if (pTexturePtr)
					{
						// set default material values
						pObjectMaterial->baseColor = XMFLOAT4(1, 1, 1, 1);
						pObjectMaterial->roughness = 0;
						pObjectMaterial->metalness = 0;
						pObjectMaterial->reflectance = 0.04f;// 0.002f;

						// first delete old textures
						if (pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource.IsValid())
						{
							pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource = {};
							pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = "";
							pObjectMaterial->SetDirty();
							wiJobSystem::context ctx;
							wiJobSystem::Wait(ctx);
						}
						if (pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource.IsValid())
						{
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = {};
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = "";
							pObjectMaterial->SetDirty();
							wiJobSystem::context ctx;
							wiJobSystem::Wait(ctx);
						}

						// set texture resource description
						TextureDesc desc;
						desc.bind_flags = BindFlag::SHADER_RESOURCE;
						desc.width = ImageWidth(pMeshTexture->iImageID);
						desc.height = ImageHeight(pMeshTexture->iImageID);
						desc.depth = 1;
						desc.mip_levels = 1;
						desc.array_size = 1;
						desc.misc_flags = ResourceMiscFlag::NONE;
						desc.usage = Usage::DEFAULT;
						desc.format = Format::R8G8B8A8_UNORM;
						desc.type = TextureDesc::Type::TEXTURE_2D;

						// get initdata ready
						std::vector<SubresourceData> InitData;

						// get access to dx11 texture desc
						DWORD bitdepth = 4;
						LPGGSURFACE pTextureInterface = NULL;
						pTexturePtr->QueryInterface<ID3D11Texture2D>(&pTextureInterface);
						D3D11_TEXTURE2D_DESC dx11desc;
						pTextureInterface->GetDesc(&dx11desc);

						// create system memory version
						ID3D11Texture2D* pTempSysMemTexture = NULL;
						D3D11_TEXTURE2D_DESC StagedDesc = { dx11desc.Width, dx11desc.Height, 1, 1, dx11desc.Format, 1, 0, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0 };
						m_pD3D->CreateTexture2D( &StagedDesc, NULL, &pTempSysMemTexture );
						if (pTempSysMemTexture)
						{
							// and copy texture image to it
							D3D11_BOX rc = { 0, 0, 0, (LONG)dx11desc.Width, (LONG)dx11desc.Height, 1 };
							m_pImmediateContext->CopySubresourceRegion(pTempSysMemTexture, 0, 0, 0, 0, pTextureInterface, 0, &rc);

							// lock for reading staging texture
							GGLOCKED_RECT d3dlock;
							if (SUCCEEDED(m_pImmediateContext->Map(pTempSysMemTexture, 0, D3D11_MAP_READ, 0, &d3dlock)))
							{
								// work out size of all data for this texture
								int iSizeOfBitmapData = dx11desc.Width*dx11desc.Height*bitdepth;

								// copy dx11 system staged texture data into initdata
								SubresourceData subresourceData;
								LPSTR pTextureMem = new char[iSizeOfBitmapData];
								memset(pTextureMem, 0, iSizeOfBitmapData);
								subresourceData.data_ptr = pTextureMem;
								subresourceData.row_pitch = dx11desc.Width * 4;
								subresourceData.slice_pitch = iSizeOfBitmapData * 4;
								InitData.push_back(subresourceData);

								// copy from surface to newly created texture mem data
								LPSTR pSrc = (LPSTR)d3dlock.pData;
								LPSTR pPtr = pTextureMem;
								DWORD dwDataWidth = dx11desc.Width*bitdepth;
								for (DWORD y = 0; y < dx11desc.Height; y++)
								{
									//PE: Get a crash here on BC3 textures. , Also got a crash on a BC1 using 12 mip levels ? (source read access)
									memcpy(pPtr, pSrc, dwDataWidth);
									pPtr += dwDataWidth;
									pSrc += d3dlock.RowPitch;
								}
								m_pImmediateContext->Unmap(pTempSysMemTexture, 0);
							}

							// free temp system surface
							SAFE_RELEASE(pTempSysMemTexture);
						}

						// release interface to original texture
						SAFE_RELEASE ( pTextureInterface );

						// give new texture a name (tests if we are freeing these registered resources)

						// PREBEN, it would seem this random name thing proves that the old named and registered images need deleting or we !!MEMORY LEAK!!
						// and also it retrieves the first image with the used name, so new images created with the same name are ignored.
						// needs closer looking at this one (used by CCP, EBE and LUA Prompt to create unique textures)

						char pMassivelyRandomTexName[MAX_PATH];
						sprintf(pMassivelyRandomTexName, "TotallyRandom%d", (int)(rand() % 99999));
						std::string sTextureName = pMassivelyRandomTexName;// "OldImagePtrTexture";
						//WickedCall_DeleteImage(sTextureName);	// leelee, this ensures only ONE copy of this created texture exists, but the engine
																// will likely require many uniquely created, so need more unique names instead of this hack!

						// create a texture and wrap it in a wiResource
						wiGraphics::Texture newTexture;
						wiGraphics::GetDevice()->CreateTexture(&desc, InitData.data(), &newTexture);
						wiGraphics::GetDevice()->SetName(&newTexture, sTextureName.c_str());
						wiResource resource;
						resource.SetTexture(newTexture);

						// create a new resource manually
						pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].name = sTextureName;
						pObjectMaterial->textures[MaterialComponent::BASECOLORMAP].resource = resource;
						if (iPutInEmissivemode == 1)
						{
							// create second emissive texture so it is lit same from any angle, and reduce baseColor influence (except its alpha mask)
							sprintf(pMassivelyRandomTexName, "TotallyRandom%d", (int)(rand() % 99999));
							sTextureName = pMassivelyRandomTexName;// "OldImagePtrTextureEmssive";

							wiGraphics::Texture newTexture2;
							wiGraphics::GetDevice()->CreateTexture(&desc, InitData.data(), &newTexture2);
							wiGraphics::GetDevice()->SetName(&newTexture2, sTextureName.c_str());
							wiResource resource2;
							resource2.SetTexture(newTexture2);

							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].name = sTextureName;
							pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource = resource2;
							//WickedCall_AddImageToList(pObjectMaterial->textures[MaterialComponent::EMISSIVEMAP].resource, IMAGERES_LEVEL, sTextureName);
							pObjectMaterial->SetEmissiveStrength(20.0f);
							pObjectMaterial->baseColor = XMFLOAT4(0, 0, 0, 1);
						}

						// inform engine of texture change here
						pObjectMaterial->SetDirty();
					}
				}
			}
		}
	}
}

void WickedCall_TextureObjectWithImagePtr(sObject* pObject, int iPutInEmissivemode)
{
	for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
	{
		sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
		if (pMesh)
		{
			WickedSetMeshNumber(iMeshIndex);
			WickedCall_TextureMeshWithImagePtr(pMesh,iPutInEmissivemode);
		}
	}
}

void WickedCall_UpdateObject(sObject* pObject)
{
	// called when pos, rot or scale updated
	uint64_t rootEntity = pObject->wickedrootentityindex;
	if ( rootEntity > 0 )
	{
		// set a transform for the object
		wiScene::TransformComponent* pTransform = wiScene::GetScene().transforms.GetComponent(rootEntity);
		if (pObject->position.bCustomWorldMatrix == false)
		{
			// start with fres transform
			pTransform->ClearTransform();

			// work out offsets of object before applying world position/rot/etc
			sFrame* pFrame = pObject->pFrame;
			if (pFrame)
			{
				// using first frame orientation to control an object wide adjustment (from importer (or from DBO))
				// rotation
				XMFLOAT3 rotinrads;
				rotinrads.x = GGToRadian(pFrame->vecRotation.x);
				rotinrads.y = GGToRadian(pFrame->vecRotation.y);
				rotinrads.z = GGToRadian(pFrame->vecRotation.z);
				XMMATRIX rot;
				rot = XMMatrixRotationX(rotinrads.x);
				rot = rot * XMMatrixRotationY(rotinrads.y);
				rot = rot * XMMatrixRotationZ(rotinrads.z);
				XMVECTOR S;
				XMVECTOR R;
				XMVECTOR T;
				XMMatrixDecompose(&S, &R, &T, rot);
				pTransform->Rotate(R);

				// work out MAX rotation matrix
				GGMATRIX matRotation;
				GGMATRIX matRotateX, matRotateY, matRotateZ;
				GGMatrixRotationX(&matRotateX, GGToRadian(pFrame->vecRotation.x));	// x rotation
				GGMatrixRotationY(&matRotateY, GGToRadian(pFrame->vecRotation.y));	// y rotation
				GGMatrixRotationZ(&matRotateZ, GGToRadian(pFrame->vecRotation.z));	// z rotation
				matRotation = matRotateX * matRotateY * matRotateZ;

				// translation (using above rotation on the offset)
				GGVECTOR3 vecPosOffset = GGVECTOR3(pFrame->vecOffset.x, pFrame->vecOffset.y, pFrame->vecOffset.z);
				GGVec3TransformCoord(&vecPosOffset, &vecPosOffset, &matRotation);
				XMFLOAT3 applyPosOffset = { 0,0,0 };
				applyPosOffset.x = vecPosOffset.x;
				applyPosOffset.y = vecPosOffset.y;
				applyPosOffset.z = vecPosOffset.z;
				pTransform->Translate(applyPosOffset);
			}
			// apply world
			pTransform->Translate(XMFLOAT3(pObject->position.vecPosition.x, pObject->position.vecPosition.y, pObject->position.vecPosition.z));
			if (pObject->bUseFixedSize)
				pTransform->Scale(XMFLOAT3(pObject->vecFixedSize.x, pObject->vecFixedSize.y, pObject->vecFixedSize.z));
			else
				pTransform->Scale(XMFLOAT3(pObject->position.vecScale.x, pObject->position.vecScale.y, pObject->position.vecScale.z));

			XMFLOAT3 rotationinrads;
			rotationinrads.x = GGToRadian(pObject->position.vecRotate.x);
			rotationinrads.y = GGToRadian(pObject->position.vecRotate.y);
			rotationinrads.z = GGToRadian(pObject->position.vecRotate.z);
			#ifndef MATCHCLASSICROTATION
			pTransform->RotateRollPitchYaw(rotationinrads);
			#else
			//PE: This will match how classic rotation work.
			XMMATRIX rot;
			rot = XMMatrixRotationX(rotationinrads.x);
			rot = rot * XMMatrixRotationY(rotationinrads.y);
			rot = rot * XMMatrixRotationZ(rotationinrads.z);
			XMVECTOR S;
			XMVECTOR R;
			XMVECTOR T;
			XMMatrixDecompose(&S, &R, &T, rot);
			pTransform->Rotate(R);
			#endif
			if (pObject->position.bApplyPivot == true)
			{
				GGMATRIX matPivot = pObject->position.matPivot;
				XMFLOAT4X4 pivotmatrix;
				pivotmatrix._11 = matPivot._11;
				pivotmatrix._12 = matPivot._12;
				pivotmatrix._13 = matPivot._13;
				pivotmatrix._14 = matPivot._14;
				pivotmatrix._21 = matPivot._21;
				pivotmatrix._22 = matPivot._22;
				pivotmatrix._23 = matPivot._23;
				pivotmatrix._24 = matPivot._24;
				pivotmatrix._31 = matPivot._31;
				pivotmatrix._32 = matPivot._32;
				pivotmatrix._33 = matPivot._33;
				pivotmatrix._34 = matPivot._34;
				pivotmatrix._41 = matPivot._41;
				pivotmatrix._42 = matPivot._42;
				pivotmatrix._43 = matPivot._43;
				pivotmatrix._44 = matPivot._44;
				XMVECTOR S, R, T;
				XMMatrixDecompose(&S, &R, &T, XMLoadFloat4x4(&pivotmatrix));
				XMFLOAT4 rotation_local;
				XMStoreFloat4(&rotation_local, R);
				pTransform->Rotate(rotation_local);
			}
		}
		else
		{
			GGMATRIX worldmatrix = pObject->position.matWorld;
			pTransform->world._11 = worldmatrix._11;
			pTransform->world._12 = worldmatrix._12;
			pTransform->world._13 = worldmatrix._13;
			pTransform->world._14 = worldmatrix._14;
			pTransform->world._21 = worldmatrix._21;
			pTransform->world._22 = worldmatrix._22;
			pTransform->world._23 = worldmatrix._23;
			pTransform->world._24 = worldmatrix._24;
			pTransform->world._31 = worldmatrix._31;
			pTransform->world._32 = worldmatrix._32;
			pTransform->world._33 = worldmatrix._33;
			pTransform->world._34 = worldmatrix._34;
			pTransform->world._41 = worldmatrix._41;
			pTransform->world._42 = worldmatrix._42;
			pTransform->world._43 = worldmatrix._43;
			pTransform->world._44 = worldmatrix._44;
			pTransform->ApplyTransform();
		}
		pTransform->UpdateTransform();
		pTransform->SetDirty();
	}
}

void WickedCall_UpdateLimbsOfObject(sObject* pObject)
{
	// called when we 'know' that we have moved limbs/frame within the object and need wicked to reflect this
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		sFrame* pFrame = pObject->ppFrameList[iF];
		if (pFrame)
		{
			Entity framewickedentity = pFrame->wickedobjindex;
			TransformComponent* pTransform = wiScene::GetScene().transforms.GetComponent(framewickedentity);
			if (pTransform)
			{
				pTransform->ClearTransform();
				pTransform->Translate(XMFLOAT3(pFrame->vecOffset.x, pFrame->vecOffset.y, pFrame->vecOffset.z));
				pTransform->Scale(XMFLOAT3(pFrame->vecScale.x, pFrame->vecScale.y, pFrame->vecScale.z));
				XMFLOAT3 rotationinrads;
				rotationinrads.x = GGToRadian(pFrame->vecRotation.x);
				rotationinrads.y = GGToRadian(pFrame->vecRotation.y);
				rotationinrads.z = GGToRadian(pFrame->vecRotation.z);
				pTransform->RotateRollPitchYaw(rotationinrads);
				pTransform->UpdateTransform();
				pTransform->SetDirty();
			}
		}
	}
}

// GGMAX 2026-08-05: mode for the widget-pick scene refresh below.
//   1 = gated (default, see the comment in WickedCall_UpdateSceneForPick)
//   2 = force a full Scene::Update every call (the pre-fix behaviour, for A/B)
//   0 = never update (diagnostic only - will break widget dragging)
// Harness: SET_PICKUPDATE <0|1|2> (strict; anything else restores 1). The run/skip
// counters below are reset by SET_PICKUPDATE but are NOT surfaced in GET_PERF_DATA -
// they exist for debugger inspection only. (An earlier comment claimed a PICKUPDATE
// line in GET_PERF_DATA that never existed.)
int g_iPickSceneUpdateMode = 1;
int g_iPickSceneUpdateRuns = 0;
int g_iPickSceneUpdateSkips = 0;

void WickedCall_UpdateSceneForPick(void)
{
	// when position/rot/etc an object, and then need to instantly
	// cast a ray to pick, need to update scene
	//PE: Only transform update needed.
	//
	// GGMAX 2026-08-05 PERF: the comment above is the DX11 intent - the fork had
	// Scene::UpdateSceneTransform(0), which ran ONLY the previous-frame-transform and
	// transform systems (WickedRepo/WickedEngine/wiScene.cpp:1593). Modern Wicked has no
	// such entry point, so the DX12 migration substituted the FULL Scene::Update(0):
	// animations, hierarchy, armatures, meshes, materials, objects+AABBs, lights, probes,
	// decals, emitters, hair, weather, occlusion and TLAS, over the whole scene
	// (island.fpm = 16,370 transforms / 8,982 objects). It is called every frame from
	// widget_getplanepos (M-Widget.cpp:390) whenever an entity is selected, and it runs
	// BEFORE the pick itself, so the Perf-P.3 pick cache cannot save it - the cheap half
	// was cached and the expensive half was left running unconditionally.
	//
	// A partial update is NOT a safe substitute here: the pick raycast tests aabb_objects,
	// which is produced by RunObjectUpdateSystem, so a transform-only refresh would leave
	// stale bounds. Instead, skip the whole thing when it provably cannot change anything -
	// the same correctness rule the pick cache already uses. The only input that moves the
	// widget plane is the camera (the callers PointObject it at the camera each frame), and
	// the only thing that moves the pick ray is the cursor. If neither moved and no mouse
	// button is held, last frame's update already left the scene in exactly this state.
	// NEVER skip while a button is down: terrain paint/sculpt and object drag all read the
	// pick-derived cursor and gate on it CHANGING each frame (see the matching note in
	// WickedCall_GetPick, wickedcalls_part3.cpp:165).
	if (g_iPickSceneUpdateMode == 0)
	{
		g_iPickSceneUpdateSkips++;
		return;
	}
	if (g_iPickSceneUpdateMode == 1)
	{
		const bool bInteracting =
			wiInput::Down(wiInput::MOUSE_BUTTON_LEFT) ||
			wiInput::Down(wiInput::MOUSE_BUTTON_RIGHT) ||
			wiInput::Down(wiInput::MOUSE_BUTTON_MIDDLE);
		if (!bInteracting)
		{
			const XMFLOAT4 ptr = wiInput::GetPointer();
			const wiScene::CameraComponent& cam = wiScene::GetCamera();
			static bool sHave = false;
			static float sM[2] = {}, sE[3] = {}, sA[3] = {}, sU[3] = {};
			if (sHave &&
				sM[0] == ptr.x && sM[1] == ptr.y &&
				sE[0] == cam.Eye.x && sE[1] == cam.Eye.y && sE[2] == cam.Eye.z &&
				sA[0] == cam.At.x && sA[1] == cam.At.y && sA[2] == cam.At.z &&
				sU[0] == cam.Up.x && sU[1] == cam.Up.y && sU[2] == cam.Up.z)
			{
				g_iPickSceneUpdateSkips++;
				return;
			}
			sHave = true;
			sM[0] = ptr.x; sM[1] = ptr.y;
			sE[0] = cam.Eye.x; sE[1] = cam.Eye.y; sE[2] = cam.Eye.z;
			sA[0] = cam.At.x;  sA[1] = cam.At.y;  sA[2] = cam.At.z;
			sU[0] = cam.Up.x;  sU[1] = cam.Up.y;  sU[2] = cam.Up.z;
		}
	}
	g_iPickSceneUpdateRuns++;
	auto gg_range = wi::profiler::BeginRangeCPU("WC-PickSceneUpdate"); // the forced full scene update, timed (the spawn stall hunt)
	wiScene::GetScene().Update(0);
	wi::profiler::EndRange(gg_range);
}

void WickedCall_SetRenderOrderBias(sMesh* pMesh, float fDistanceToAdd)
{
	sFrame* pFrame = pMesh->pFrameAttachedTo;
	if (pFrame)
	{
		ObjectComponent* object = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
		if (object)
		{
			object->sort_priority = (int)fDistanceToAdd;
		}
	}
}

float WickedCall_GetRenderOrderBias(sMesh* pMesh)
{
	sFrame* pFrame = pMesh->pFrameAttachedTo;
	if (pFrame)
	{
		ObjectComponent* object = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
		if (object)
		{
			return (float)object->sort_priority;
		}
	}
	return 0; // compiler complains if this is missing, what's a suitable default value?
}

void WickedCall_SetLimbVisible(sFrame* pFrame, bool bVisible)
{
	if (pFrame)
	{
		ObjectComponent* object = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
		if (object)
		{
			object->SetRenderable(bVisible);
		}
		//PE: Hide the mesh , or it will have no effect.
		sMesh* pMesh = pFrame->pMesh;
		if (pMesh)
		{
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				mesh->SetRenderable(bVisible);
			}
		}
	}
}

void WickedCall_SetObjectPreventAnyApparentOcclusion (sObject* pObject, bool bPreventAnyApparentOcclusion)
{
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		sFrame* pFrame = pObject->ppFrameList[iF];
		if(pFrame)
		{
			if (pFrame->wickedobjindex > 0)
			{
				ObjectComponent* object = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
				if (object)
				{
					object->SetApparentCullDisabled(bPreventAnyApparentOcclusion); // the engine's apparent-size cull leaves it in view (ObjectComponent::APPARENT_CULL_DISABLED)
				}
			}
		}
	}
}

void WickedCall_SetDisableCollision(sObject* pObject,bool collision)
{
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		sFrame* pFrame = pObject->ppFrameList[iF];
		if (pFrame)
		{
			if (pFrame->wickedobjindex > 0)
			{
				ObjectComponent* object = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
				if (object)
				{
					//object->SetDisableCollision(collision); // REMOVED
				}
			}
		}
	}
}

void WickedCall_SetObjectVisible ( sObject* pObject, bool bVisible )
{
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		bool bVisForThisFrame = bVisible;
		sFrame* pFrame = pObject->ppFrameList[iF];
		if (pFrame->pMesh && pFrame->pMesh->bVisible == false ) bVisForThisFrame = false; // if limb was hiddem keep hidden until otherwise
		WickedCall_SetLimbVisible(pFrame, bVisForThisFrame);
	}
}

void WickedCall_GlueObjectToObject(sObject* pObjectToGlue, sObject* pParentObject, int iLimb, int iObjIDToSyncAnimTo, int iWorldToLocal)
{
	// attaches this entity object to a parent entity object
	if ( pObjectToGlue && pParentObject )
	{
		uint64_t rootToGlueEntity = pObjectToGlue->wickedrootentityindex;
		if (rootToGlueEntity > 0)
		{
			if (iLimb < pParentObject->iFrameCount)
			{
				sFrame* pFrame = pParentObject->ppFrameList[iLimb];
				uint64_t objectParentToAttachTo = pFrame->wickedobjindex;
				if (rootToGlueEntity > 0)
				{
					// attach child to parent
					bool bAlreadyInChildPosition = false; // i.e. offset to this parent, not world position
					if (iWorldToLocal == 1) bAlreadyInChildPosition = true;
					wiScene::GetScene().Component_Attach(rootToGlueEntity, objectParentToAttachTo, bAlreadyInChildPosition);

					// additionally assign child an ability to perfectly sync anim with parent
					if (pObjectToGlue->pAnimationSet)
					{
						// first animset only for frame return
						sAnimationSet* pAnimSet = pObjectToGlue->pAnimationSet;
						if (pAnimSet)
						{
							Entity animentity = pAnimSet->wickedanimentityindex;
							AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(animentity);
							if (animationcomponent)
							{
								if (iObjIDToSyncAnimTo != -1)
								{
									// find the animation component of the parent to assign to this child animation 
									sObject* GetObjectData(int iID);
									sObject* pParentObjWithAnim = GetObjectData(iObjIDToSyncAnimTo);
									if (pParentObjWithAnim->pAnimationSet)
									{
										// first animset only for frame return
										sAnimationSet* pParentAnimSet = pParentObjWithAnim->pAnimationSet;
										if (pParentAnimSet)
										{
											Entity parentanimentity = pParentAnimSet->wickedanimentityindex;
											AnimationComponent* parentanimationcomponent = wiScene::GetScene().animations.GetComponent(animentity);
											if (parentanimationcomponent)
											{
												////GGAnimBridge_SetPrimaryAnimSync(animentity, parentanimentity);
											}
										}
									}
								}
								else
								{
									// unsync child from parent
									////GGAnimBridge_ClearPrimaryAnimSync(animentity);
								}
							}
						}
					}
				}
			}
		}
	}
}

void WickedCall_UnGlueObjectToObject(sObject* pObjectToUnGlue)
{
	// deattaches entity object from any parent
	if ( pObjectToUnGlue )
	{
		uint64_t rootToUnGlueEntity = pObjectToUnGlue->wickedrootentityindex;
		if (rootToUnGlueEntity > 0)
		{
			wiScene::GetScene().Component_Detach(rootToUnGlueEntity);
			if (pObjectToUnGlue->pAnimationSet)
			{
				sAnimationSet* pAnimSet = pObjectToUnGlue->pAnimationSet;
				if (pAnimSet)
				{
					Entity animentity = pAnimSet->wickedanimentityindex;
					AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(animentity);
					if (animationcomponent)
					{
						////GGAnimBridge_ClearPrimaryAnimSync(animentity);
					}
				}
			}
		}
	}
}

void WickedCall_PresetObjectRenderLayer(int iLayerMask)
{
	g_iWickedLayerMaskPreference = iLayerMask;
	g_iWickedLayerMaskOptionalLimb = -1;
}

void WickedCall_PresetObjectLimbRenderLayer(int iLayerMask, int iLimb)
{
	g_iWickedLayerMaskPreference = iLayerMask;
	g_iWickedLayerMaskOptionalLimb = iLimb;
}

void WickedCall_PresetObjectUVScale(float fUScale, float fVScale)
{
	g_iWickedUScalePreference = fUScale;
	g_iWickedVScalePreference = fVScale;
}

void WickedCall_PresetObjectCreateOnDemand(bool bCreateOnlyWhenUsed)
{
	// for some objects, like explosion planes, we have avoided creating
	// a wicked object for it to save on engine management, and will accept
	// the small hit of the objects creation until we entirely replace
	// all decal/particle systems with a GPU particle system
	g_bWickedCreateOnlyWhenUsed = bCreateOnlyWhenUsed;
}

void WickedCall_PresetObjectIgnoreTextures(bool bIgnoreTextureInfo)
{
	// used when we know DBOs have textures but we do not want to try and load any
	// such as the widget DBO objects which carry some rogue old material names
	g_bWickedIgnoreTextureInfo = bIgnoreTextureInfo;
}

void WickedCall_PresetObjectTextureFromImagePtr(bool bUseImagePtrInsteadOfTexFile, int iPutInEmissiveMode)
{
	// use this flag when you want to force a TextureObject function to use the ImagePtr to source the texture
	// rather than the texture filename stored in the texture structure of the mesh (the default behavior)
	g_bWickedUseImagePtrInsteadOfTexFile = bUseImagePtrInsteadOfTexFile;
	g_iWickedPutInEmissiveMode = iPutInEmissiveMode;
}

void WickedCall_PresetObjectPutInEmissive(int iPutInEmissiveMode)
{
	g_iWickedPutInEmissiveMode = iPutInEmissiveMode;
}

void WickedCall_RotateLimb(sObject* pObject, sFrame* pFrame, float fAX, float fAY, float fAZ)
{
	if (pFrame->pAnimRef)
	{
		if (pObject->pAnimationSet)
		{
			uint64_t wickedanimationindex = pObject->pAnimationSet->wickedanimentityindex;
			AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(wickedanimationindex);
			if (animationcomponent)
			{
				int iIndex = pFrame->pAnimRef->wickedanimationchannel[1];
				if (iIndex >= 0)
				{
					AnimationComponent::AnimationChannel* pAnimationChannel = &animationcomponent->channels[iIndex];
					if (pAnimationChannel)
					{
						// Modify rotation keyframes directly so the animation system
						// evaluates them and the armature picks up the result in the
						// normal single pass (no PostUpdate re-run needed).
						////XMFLOAT4 rot;
						////XMStoreFloat4(&rot, XMQuaternionRotationRollPitchYaw(GGToRadian(fAX), GGToRadian(fAY), GGToRadian(fAZ)));
						////int rotSamplerIdx = pAnimationChannel->samplerIndex;
						////GGAnimBridge_ApplyAdditiveRotation(&wiScene::GetScene(), wickedanimationindex, rotSamplerIdx, rot);
						pAnimationChannel->iUsePreFrame = 1;
						pAnimationChannel->qPreFrameRotation = XMQuaternionRotationRollPitchYaw(GGToRadian(fAX), GGToRadian(fAY), GGToRadian(fAZ));
					}
				}
			}
		}
	}
}

void WickedCall_CalculateWorldQuat(GGVECTOR4* pQuatA, GGVECTOR4* pQuatB, GGVECTOR4* pQuatResult)
{
	XMVECTOR qQuatA = XMLoadFloat4(&XMFLOAT4(pQuatA->x, pQuatA->y, pQuatA->z, pQuatA->w));
	XMVECTOR qQuatB = XMLoadFloat4(&XMFLOAT4(pQuatB->x, pQuatB->y, pQuatB->z, pQuatB->w));
	XMVECTOR qResult = XMQuaternionMultiply(qQuatA, qQuatB);
	pQuatResult->x = qResult.m128_f32[0];
	pQuatResult->y = qResult.m128_f32[1];
	pQuatResult->z = qResult.m128_f32[2];
	pQuatResult->w = qResult.m128_f32[3];
}

void WickedCall_CalculateQuatDiff(GGVECTOR4* pQuatA, GGVECTOR4* pQuatB, GGVECTOR4* pQuatDiff)
{
	XMVECTOR qQuatA = XMLoadFloat4(&XMFLOAT4(pQuatA->x, pQuatA->y, pQuatA->z, pQuatA->w));
	XMVECTOR qQuatB = XMLoadFloat4(&XMFLOAT4(pQuatB->x, pQuatB->y, pQuatB->z, pQuatB->w));
	qQuatA = XMQuaternionInverse(qQuatA);
	XMVECTOR qRotationDiff = XMQuaternionMultiply(qQuatA, qQuatB);
	pQuatDiff->x = qRotationDiff.m128_f32[0];
	pQuatDiff->y = qRotationDiff.m128_f32[1];
	pQuatDiff->z = qRotationDiff.m128_f32[2];
	pQuatDiff->w = qRotationDiff.m128_f32[3];
}

void WickedCall_CalculateQuatFromCombined(sObject* pObject, sFrame* pFrame, GGVECTOR4* pQuat)
{
	GGMATRIX matCombined = pFrame->matCombined;
	XMFLOAT4X4 matLimbRot;
	matLimbRot._11 = matCombined._11;
	matLimbRot._12 = matCombined._12;
	matLimbRot._13 = matCombined._13;
	matLimbRot._14 = matCombined._14;
	matLimbRot._21 = matCombined._21;
	matLimbRot._22 = matCombined._22;
	matLimbRot._23 = matCombined._23;
	matLimbRot._24 = matCombined._24;
	matLimbRot._31 = matCombined._31;
	matLimbRot._32 = matCombined._32;
	matLimbRot._33 = matCombined._33;
	matLimbRot._34 = matCombined._34;
	matLimbRot._41 = 0;
	matLimbRot._42 = 0;
	matLimbRot._43 = 0;
	matLimbRot._44 = 0;
	XMMATRIX finalMat = XMLoadFloat4x4(&matLimbRot);
	XMVECTOR qRotation = XMQuaternionRotationMatrix(finalMat);
	pQuat->x = qRotation.m128_f32[0];
	pQuat->y = qRotation.m128_f32[1];
	pQuat->z = qRotation.m128_f32[2];
	pQuat->w = qRotation.m128_f32[3];
}

// GGMAX 2.29 (2026-08-12): DUMP_RAGDOLL instrument — WRITEBACK stage (stage 3 of 3).
// Each counter is one nesting level deeper than the last, so the harness reads off exactly where
// the writeback stops mattering. ★ g_rdWBApplied is the one that settles it: everything above it
// can be healthy while it stays 0, because the two GGAnimBridge_SetPreFrame calls that would
// actually move a bone are commented out (`////`) — as is EVERY other call site of the PreFrame
// API in this file, even though GGAnimBridge.cpp implements it and consumes it live in PostUpdate.
int g_rdWBEntered  = 0;  // function called at all
int g_rdWBAnimRef  = 0;  // pFrame->pAnimRef present
int g_rdWBAnimSet  = 0;  // pObject->pAnimationSet present
int g_rdWBAnimComp = 0;  // AnimationComponent found in the scene
int g_rdWBChannel  = 0;  // animation channels resolved — the apply point is reached
int g_rdWBApplied  = 0;  // a bone override was actually issued
int g_rdWBSplitTgt = 0;  // pos/rot channels targeted DIFFERENT entities (see below)

// GGMAX 2.29: what the writeback last WROTE, so DUMP_RAGDOLL can read the same bone back out of
// the scene and compare. ★ This is the camera-free proof that the pose reaches the mesh — the
// obvious check (ragdoll someone and screenshot) is void whenever the body is off-camera, which
// is exactly what the first 2.29 run produced: a 0.018%-different image of an empty room.
uint64_t g_rdWBLastTarget = 0;
XMFLOAT3 g_rdWBLastTrans = XMFLOAT3(0, 0, 0);
XMFLOAT4 g_rdWBLastRot = XMFLOAT4(0, 0, 0, 1);

// ⚠ …and the read-back alone is NOT conclusive when it says HELD. The harness runs inside
// GuruLoopLogic, i.e. in the same game-logic phase as the Bullet step, so a dump can sample the
// bone between our write and the next Scene::Update and see its own value regardless of whether
// the animation system clobbers it a moment later. g_rdWBClobbered closes that hole and is
// phase-independent: on the NEXT writeback for the same bone, before overwriting it, compare
// what is there now against what we left last time. A rising count means something restored the
// bone between writebacks — a still-playing animation clip being the prime suspect
// (DBProRagDoll::Update ends with LoopObject(objectID,0,1), which keeps the clip running).
int g_rdWBClobbered = 0;
int g_rdWBSurvived = 0;
int g_rdWBTrackedWritten = 0;   // the latched bone has been written at least once

// GGMAX 2.29: ragdoll bone writeback, 1 = live (default), 0 = the 2.28 no-op behaviour.
// setup.ini `ragdollwriteback=0` / harness SET_RAGDOLLWRITEBACK 0 reverts.
int g_ragdollWriteback = 1;
void GGSetRagdollWriteback(int v) { g_ragdollWriteback = (v != 0) ? 1 : 0; }

void WickedCall_OverrideLimbWithCombined(sObject* pObject, sFrame* pFrame, bool bIncludeTranslation)
{
	g_rdWBEntered++;
	if (pFrame->pAnimRef)
	{
		g_rdWBAnimRef++;
		if (pObject->pAnimationSet)
		{
			g_rdWBAnimSet++;
			uint64_t wickedanimationindex = pObject->pAnimationSet->wickedanimentityindex;
			AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(wickedanimationindex);
			if (animationcomponent)
			{
				g_rdWBAnimComp++;
				int iIndexPos = pFrame->pAnimRef->wickedanimationchannel[0];
				int iIndexRot = pFrame->pAnimRef->wickedanimationchannel[1];
				if (iIndexPos >= 0)//&& iIndexPos < animationcomponent->channels.size() && iIndexRot >= 0 && iIndexRot < animationcomponent->channels.size())
				{
					AnimationComponent::AnimationChannel* pAnimationChannelPos = &animationcomponent->channels[iIndexPos];
					AnimationComponent::AnimationChannel* pAnimationChannelRot = &animationcomponent->channels[iIndexRot];
					if (pAnimationChannelPos && pAnimationChannelRot)
					{
						g_rdWBChannel++;
						// Extract rotation from combined matrix
						GGMATRIX matCombined = pFrame->matCombined;
						XMFLOAT4X4 mat44;
						mat44._11 = matCombined._11; mat44._12 = matCombined._12; mat44._13 = matCombined._13; mat44._14 = 0;
						mat44._21 = matCombined._21; mat44._22 = matCombined._22; mat44._23 = matCombined._23; mat44._24 = 0;
						mat44._31 = matCombined._31; mat44._32 = matCombined._32; mat44._33 = matCombined._33; mat44._34 = 0;
						mat44._41 = 0; mat44._42 = 0; mat44._43 = 0; mat44._44 = 1;
						XMFLOAT4 currentRot;
						XMStoreFloat4(&currentRot, XMQuaternionRotationMatrix(XMLoadFloat4x4(&mat44)));
						XMFLOAT3 trans(pFrame->matCombined._41, pFrame->matCombined._42, pFrame->matCombined._43);

						// GGMAX 2.29: ONE call, carrying translation AND rotation together.
						//
						// The two disabled lines this replaces were a faithful-looking port of DX11
						// (wickedcalls.cpp:5055-5082 in the DX11 tree), which sets iUsePreFrame=2 on
						// the POS channel with the translation and on the ROT channel with the
						// rotation. That works there because the preframe lived ON THE CHANNEL and
						// the engine applied each channel down its own path. The DX12 bridge keys
						// preframes by channel->target — the BONE ENTITY — and both channels of a
						// bone share one target, so the second call OVERWRITES the first. Enabling
						// them as written would have applied rotation with an IDENTITY translation
						// and mode 2 lerps translation, i.e. every ragdoll bone dragged to its
						// parent's origin. Very possibly why they were commented out rather than
						// fixed. Merging into a single SetPreFrame restores the DX11 behaviour.
						// ⚠ That merge is only valid while pos->target == rot->target — so the
						// instrument COUNTS violations (g_rdWBSplitTgt) instead of trusting it;
						// a non-zero there in DUMP_RAGDOLL invalidates this call, read it first.
						// Note bIncludeTranslation is unused, exactly as in DX11: that tree ignores
						// the parameter too and always writes translation.
						if (pAnimationChannelPos->target != pAnimationChannelRot->target) g_rdWBSplitTgt++;
						if (g_ragdollWriteback)
						{
							// ★ NOT GGAnimBridge_SetPreFrame — ITS CONSUMER IS DEAD. The bridge
							// stores preframes in a map that only GGAnimBridge_PostUpdate reads,
							// and that call is commented out in MasterRenderer::PostUpdate
							// (master_part1.cpp:617, disabled alongside PreUpdate in 89873913 as
							// "too slow"). Routing through it applies nothing — a counter would
							// have read "applied" while the character stayed T-posed, which is
							// the exact failure mode this instrument exists to expose.
							// Even alive it would be the wrong hook: PostUpdate runs AFTER
							// Scene::Update, so the animation system re-samples the bone next
							// frame before the armature pass ever sees the override.
							// Writing the bone's LOCAL transform here is correct instead: this
							// runs inside the Bullet step, in the game-logic phase, so the write
							// lands before this frame's Scene::Update and the armature picks it
							// up in the normal single pass.
							wi::scene::TransformComponent* bone =
								wiScene::GetScene().transforms.GetComponent(pAnimationChannelRot->target);
							if (bone != nullptr)
							{
								// Phase-independent survival check (see g_rdWBClobbered above):
								// for the one bone we track, did our value still hold when we
								// came back to write it again?
								// ⚠ The tracked bone must be LATCHED. The first cut recorded
								// "last bone written", which is a different bone every call, so
								// the comparison never fired and the run reported 0/0 — a
								// silent, plausible-looking nothing. Latch on first use instead;
								// the same bone then comes round once per ragdoll update.
								if (g_rdWBLastTarget == 0)
									g_rdWBLastTarget = (uint64_t)pAnimationChannelRot->target;
								const bool bTracked =
									((uint64_t)pAnimationChannelRot->target == g_rdWBLastTarget);
								if (bTracked && g_rdWBTrackedWritten)
								{
									const float drift =
										fabsf(bone->rotation_local.x - g_rdWBLastRot.x) +
										fabsf(bone->rotation_local.y - g_rdWBLastRot.y) +
										fabsf(bone->rotation_local.z - g_rdWBLastRot.z) +
										fabsf(bone->rotation_local.w - g_rdWBLastRot.w);
									if (drift > 0.0005f) g_rdWBClobbered++; else g_rdWBSurvived++;
								}
								// Rotation on every bone; translation ONLY on the root joint.
								// ⚠ This DIVERGES from DX11, deliberately. DX11 wrote the
								// translation for every bone (and ignored bIncludeTranslation
								// entirely), but there the value went into a preframe the engine
								// applied in the animation channel's own space. Here it lands
								// straight on a Wicked bone local, where the rest-pose offset is
								// what sets bone LENGTH — pushing a possibly mis-scaled
								// translation into all 66 bones risks a stretched skeleton, and
								// a ragdoll pose is carried by rotations anyway. The root still
								// needs its translation or the body cannot leave its spawn spot,
								// which is exactly what bIncludeTranslation marks
								// (DBProRagDoll.cpp: true only when iF == m_iAnimateFromJoint).
								if (bIncludeTranslation) bone->translation_local = trans;
								bone->rotation_local = currentRot;
								bone->SetDirty();
								g_rdWBApplied++;
								if (bTracked)
								{
									// Record the POST-WRITE state, not `trans` — on a non-root
									// bone the translation is deliberately left alone, and
									// comparing against a value we never wrote would report a
									// false "OVERWRITTEN" on every dump.
									g_rdWBLastTrans = bone->translation_local;
									g_rdWBLastRot = currentRot;
									g_rdWBTrackedWritten = 1;
								}
							}
						}
					}
				}
			}
		}
	}
}

void WickedCall_OverrideLimbOff(sObject* pObject, sFrame* pFrame)
{
	if (pFrame->pAnimRef)
	{
		if (pObject->pAnimationSet)
		{
			uint64_t wickedanimationindex = pObject->pAnimationSet->wickedanimentityindex;
			AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(wickedanimationindex);
			if (animationcomponent)
			{
				int iIndexPos = pFrame->pAnimRef->wickedanimationchannel[0];
				int iIndexRot = pFrame->pAnimRef->wickedanimationchannel[1];
				if (iIndexPos >= 0)//&& iIndexPos < animationcomponent->channels.size() && iIndexRot >= 0 && iIndexRot < animationcomponent->channels.size())
				{
					AnimationComponent::AnimationChannel* pAnimationChannelPos = &animationcomponent->channels[iIndexPos];
					AnimationComponent::AnimationChannel* pAnimationChannelRot = &animationcomponent->channels[iIndexRot];
					// GGMAX 2.29: the matching release. Nothing to undo now that the writeback
					// pokes the bone transform directly instead of parking an override in the
					// (dead) preframe map — the animation system reclaims the bone as soon as it
					// evaluates again. Both ClearPreFrame calls stay disabled deliberately: the
					// map has no live producer and no live consumer, so calling it would only
					// suggest a mechanism that is not running.
					// ////if (pAnimationChannelPos) GGAnimBridge_ClearPreFrame(pAnimationChannelPos->target);
					// ////if (pAnimationChannelRot) GGAnimBridge_ClearPreFrame(pAnimationChannelRot->target);
					(void)pAnimationChannelPos; (void)pAnimationChannelRot;
				}
			}
		}
	}
}

void WickedCall_SetBip01Position(sObject* pObject, sFrame* pFrame, int iUseMode, float fX, float fZ)
{
	if (pFrame->pAnimRef)
	{
		if (pObject->pAnimationSet)
		{
			uint64_t wickedanimationindex = pObject->pAnimationSet->wickedanimentityindex;
			AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(wickedanimationindex);
			if (animationcomponent)
			{
				int iIndexPos = pFrame->pAnimRef->wickedanimationchannel[0];
				if (iIndexPos >= 0)//&& iIndexPos < animationcomponent->channels.size() )
				{
					AnimationComponent::AnimationChannel* pAnimationChannel = &animationcomponent->channels[iIndexPos];
					if (pAnimationChannel)
					{
						if (iUseMode == 3)
						{
							pAnimationChannel->iUsePreFrame = 3;
							pAnimationChannel->vPreFrameTranslation = XMVectorSet(0, 0, 0, 0);
						}
						else
						{
							pAnimationChannel->iUsePreFrame = 0;
						}
					}
				}
			}
		}
	}
}

void WickedCall_SetBip01PositionDX12(sObject* pObject, sFrame* pFrame, int iUseMode, float fX, float fZ)
{
	if (pFrame->pAnimRef)
	{
		if (pObject->pAnimationSet)
		{
			uint64_t wickedanimationindex = pObject->pAnimationSet->wickedanimentityindex;
			AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(wickedanimationindex);
			if (animationcomponent)
			{
				int iIndexPos = pFrame->pAnimRef->wickedanimationchannel[0];
				if (iIndexPos >= 0)//&& iIndexPos < animationcomponent->channels.size() )
				{
					AnimationComponent::AnimationChannel* pAnimationChannel = &animationcomponent->channels[iIndexPos];
					if (pAnimationChannel)
					{
						if (iUseMode == 3)
						{
							// Zero Bip01 X/Z in the animation keyframe data itself so the
							// engine's normal pipeline produces (0, Y, 0). This prevents
							// double-movement (root motion extraction + bone animation drift).
							int samplerIdx = pAnimationChannel->samplerIndex;
							////GGAnimBridge_ZeroBip01TranslationXZ(&wiScene::GetScene(), wickedanimationindex, samplerIdx);

							// Freeze Bip01 rotation keyframes to the first keyframe's value
							// so all animation sections produce the same rotation. Capture the
							// base rotation to use in the preframe -- this ensures the preframe
							// and keyframes agree, preventing slerp mismatch during amount < 1
							// transitions (which caused the 90-degree snap at walk->idle).
							XMFLOAT4 baseRotation = XMFLOAT4(0, 0, 0, 1);
							int iIndexRot = pFrame->pAnimRef->wickedanimationchannel[1];
							if (iIndexRot >= 0)
							{
								AnimationComponent::AnimationChannel* pRotChannel = &animationcomponent->channels[iIndexRot];
								if (pRotChannel)
								{
									int rotSamplerIdx = pRotChannel->samplerIndex;
									////GGAnimBridge_ZeroBip01Rotation(&wiScene::GetScene(), wickedanimationindex, rotSamplerIdx, &baseRotation);
								}
							}

							////GGAnimBridge_SetPreFrame(pAnimationChannel->target, 3, 1.0f, XMFLOAT3(0, 0, 0), baseRotation, XMFLOAT3(1, 1, 1));
						}
						else
						{
							// Restore original Bip01 X/Z keyframe data
							int samplerIdx = pAnimationChannel->samplerIndex;
							////GGAnimBridge_RestoreBip01TranslationXZ(&wiScene::GetScene(), wickedanimationindex, samplerIdx);

							// Restore original Bip01 rotation keyframe data
							int iIndexRot = pFrame->pAnimRef->wickedanimationchannel[1];
							if (iIndexRot >= 0)
							{
								AnimationComponent::AnimationChannel* pRotChannel = &animationcomponent->channels[iIndexRot];
								if (pRotChannel)
								{
									int rotSamplerIdx = pRotChannel->samplerIndex;
									/////GGAnimBridge_RestoreBip01Rotation(&wiScene::GetScene(), wickedanimationindex, rotSamplerIdx);
								}
							}

							/////GGAnimBridge_ClearPreFrame(pAnimationChannel->target);
						}
					}
				}
			}
		}
	}
}

void WickedCall_SetBip01Rotation(sObject* pObject, sFrame* pFrame, int iUseMode, float fY)
{
	if (pFrame->pAnimRef)
	{
		if (pObject->pAnimationSet)
		{
			uint64_t wickedanimationindex = pObject->pAnimationSet->wickedanimentityindex;
			AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(wickedanimationindex);
			if (animationcomponent)
			{
				int iIndex = pFrame->pAnimRef->wickedanimationchannel[1];
				if (iIndex >= 0)//&& iIndex < animationcomponent->channels.size())
				{
					AnimationComponent::AnimationChannel* pAnimationChannel = &animationcomponent->channels[iIndex];
					if (pAnimationChannel)
					{
						if (iUseMode > 0)
						{
							XMFLOAT4 rot;
							XMStoreFloat4(&rot, XMQuaternionRotationRollPitchYaw(0, 0, 0));
							////GGAnimBridge_SetPreFrame(pAnimationChannel->target, iUseMode, 1.0f,	XMFLOAT3(0, 0, 0), rot, XMFLOAT3(1, 1, 1));
						}
						else
						{
							////GGAnimBridge_ClearPreFrame(pAnimationChannel->target);
						}
					}
				}
			}
		}
	}
}

std::vector<sFrame*> g_pFramesToAffect;
void AddToFramesToAffect(sFrame* pFrame)
{
	if (pFrame)
	{
		g_pFramesToAffect.push_back(pFrame);
		AddToFramesToAffect(pFrame->pChild);
		AddToFramesToAffect(pFrame->pSibling);
	}
}

void WickedCall_SetObjectPreFrames(sObject* pObject, LPSTR pParentFrameName, float fFrameToUse, float fSmoothSlerpToNextShape, int iPreFrameMode)
{
	// seems a better way is to 'reuse' the animation system and specify the frame time there instead
	// of here as do not always get the exact frame due to different ways to calculate the final frame
	// so perhaps change the SRT vectors stored with keyframe and add more to wicked animation system
	fFrameToUse += 1.0f;

	// scan all frames in object, but only affect those after the parent specified (i.e. all frames after the head frame)
	sFrame* pTargetParentFrame = NULL;
	for (int iFrame = 0; iFrame < pObject->iFrameCount; iFrame++)
	{
		sFrame* pFrame = pObject->ppFrameList[iFrame];
		if (pFrame)
		{
			if (stricmp(pFrame->szName, pParentFrameName) == NULL)
			{
				pTargetParentFrame = pFrame; break;
			}
		}
	}
	if (pTargetParentFrame)
	{
		// collect all frames affected
		g_pFramesToAffect.clear();
		AddToFramesToAffect(pTargetParentFrame->pChild);

		// get animation component
		uint64_t wickedanimationindex = pObject->pAnimationSet->wickedanimentityindex;
		AnimationComponent* animationcomponent = wiScene::GetScene().animations.GetComponent(wickedanimationindex);
		if (animationcomponent)
		{
			// for each frame, copy the specified animation frame to the preframe
			for (int iF = 0; iF < g_pFramesToAffect.size(); iF++)
			{
				sFrame* pFrame = g_pFramesToAffect[iF];
				sAnimation* pAnim = pFrame->pAnimRef;
				if (pAnim)
				{
					// must loop through three channels and samplers for this frame (T then R then S)
					for (int i = 0; i < 3; i++)
					{
						// LB: new animation system in wicked wipes out "backwards_compatibility_data" so need to use
						// the new location for "keyframe_times, etc"
						// prevent crash to ensure pAnim->wickedanimationchannel[i] not -1
						if (pAnim->wickedanimationchannel[i] >= 0 && pAnim->wickedanimationsampler[i] >= 0)
						{
							// for this animation, locate the channels and samplers for the frame
							AnimationComponent::AnimationChannel* pAnimationChannel = &animationcomponent->channels[pAnim->wickedanimationchannel[i]];
							AnimationComponent::AnimationSampler* pAnimationSampler = &animationcomponent->samplers[pAnim->wickedanimationsampler[i]];
							if (pAnimationChannel && pAnimationSampler)
							{
								AnimationDataComponent* animationdata = wiScene::GetScene().animation_datas.GetComponent(pAnimationSampler->data);
								if (animationdata == nullptr)
									continue;

								// find true keyframe slot using frame time passed in (using fFrameToUse)
								int keyLeft = 0;
								int keyRight = 0;
								if (animationdata->keyframe_times.back() < fFrameToUse)
								{
									// Rightmost keyframe is already outside animation, so just snap to last keyframe:
									keyLeft = keyRight = (int)animationdata->keyframe_times.size() - 1;
								}
								else
								{
									// Search for the right keyframe (greater/equal to anim time):
									while (animationdata->keyframe_times[keyRight++] < fFrameToUse) {}
									keyRight--;
									// Left keyframe is just near right:
									keyLeft = std::max(0, keyRight - 1);
								}

								float fSmooth = 1.0f / fSmoothSlerpToNextShape;
								XMFLOAT3 trans(0, 0, 0);
								XMFLOAT4 rot(0, 0, 0, 1);
								XMFLOAT3 scl(1, 1, 1);
								if (i == 0)
									trans = ((const XMFLOAT3*)animationdata->keyframe_data.data())[keyLeft];
								if (i == 1)
									rot = ((const XMFLOAT4*)animationdata->keyframe_data.data())[keyLeft];
								if (i == 2)
									scl = ((const XMFLOAT3*)animationdata->keyframe_data.data())[keyLeft];
								////GGAnimBridge_SetPreFrame(pAnimationChannel->target, iPreFrameMode, fSmooth,	trans, rot, scl);
							}
						}
					}
				}
			}
		}
	}
}

void WickedCall_SetObjectRenderLayer(sObject* pObject,int iLayerMask)
{
	// only for objects that are NOT glued
	//if (pObject->position.bGlued == false && pObject->position.iBeenGluedToBy == 0)
	if ( pObject->position.iBeenGluedToBy == 0)
	{
		// GGMAX 2.48: the old note here read "this does not work, cannot seem to set the layermask
		// AFTER you have created the object and merged it with the scene!" — root-caused at last.
		// Scene::Intersects (the raycast behind wiScene::Pick) does not test this LIVE
		// LayerComponent; it tests the CACHED copy in aabb_objects[i].layerMask (engine
		// wiScene.cpp:7180), and that cache is only rewritten from the live layer during
		// Scene::Update (wiScene.cpp:5516). So a persistent layer change works from the next
		// frame on, but the swap-pick-restore pattern (IntersectAllEx excludes the player's gun
		// and its ignore-object this way, CObjectsC_part3.cpp:1780-1786) happens entirely BETWEEN
		// updates and was a silent no-op on DX12 — the weapon stayed pickable, and a zoomed shot
		// (gun pulled onto the camera axis) was swallowed by the player's own weapon mesh:
		// no damage, no bullet hole, no impact, while ammo/flash/sound all still ran.
		// DX11's Pick read the live component (WickedRepo wiScene.cpp:4989), which is why the
		// identical game code worked there. Fix: write the cache through alongside the live
		// component. Costs a GetIndex per frame entity at SWAP time only — the per-object pick
		// loop is untouched. Next Scene::Update re-derives the cache from the live layer, so the
		// two can never drift.
		for (int iF = 0; iF < pObject->iFrameCount; iF++)
		{
			uint64_t objectEntity = pObject->ppFrameList[iF]->wickedobjindex;
			if (objectEntity > 0)
			{
				wiScene::LayerComponent* pWickedLayer = wiScene::GetScene().layers.GetComponent(objectEntity);
				if (pWickedLayer == nullptr) continue;
				pWickedLayer->layerMask = iLayerMask;

				//PE: layermask is taken from the parent in the hierarchy
				HierarchyComponent* parent = wiScene::GetScene().hierarchy.GetComponent(objectEntity);
				if (parent != nullptr)
				{
					parent->layerMask_bind = iLayerMask;
				}

				// GGMAX 2.48: write-through to the pick cache (see block comment above)
				wiScene::Scene& ggScene = wiScene::GetScene();
				size_t ggObjIdx = ggScene.objects.GetIndex(objectEntity);
				if (ggObjIdx != ~0ull && ggObjIdx < ggScene.aabb_objects.size())
				{
					ggScene.aabb_objects[ggObjIdx].layerMask = (uint32_t)iLayerMask;
				}
			}
		}
	}
}


bool Convert2Dto3D(long x , long y, float* pOutX, float* pOutY, float* pOutZ, float* pDirX, float* pDirY, float* pDirZ)
{
	//PE: Wicked Mouse is relative to windows pos. ImGui is relative to screen.
	RAY pickRay = wiRenderer::GetPickRay((long)x, (long)y, master.masterrenderer);
	*pOutX = pickRay.origin.x;
	*pOutY = pickRay.origin.y;
	*pOutZ = pickRay.origin.z;
	*pDirX = pickRay.direction.x;
	*pDirY = pickRay.direction.y;
	*pDirZ = pickRay.direction.z;
	return true;
}

float fLastTerrainHitX = 0, fLastTerrainHitY = 0, fLastTerrainHitZ = 0;

#ifdef PICKBVHTHREADED
//PE: To compare only.
bool WickedCall_GetPick2_BVH(float fMouseX, float fMouseY, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask)
{
	// Use wicked mouse pointer to determine intersection with solid geometry (for terrain and entity detection)
	// PE: We got hits from hidden objects like "widgets" , now ignore hidden objects in WICKEDREPO.
	bool bHitSuccess = false;

	//PE: Do not do anything if mouse is not over level editor.
	if (bImGuiGotFocus) return bHitSuccess;

	// do not wipe out hover object detection when widget plane pass happens
	if (iLayerMask & GGRENDERLAYERS_NORMAL)
	{
		g_hovered_pobject = NULL;
		g_hovered_entity = 0;
	}

	//PE: Wicked Mouse is relative to windows pos. ImGui is relative to screen.
	RAY pickRay = wiRenderer::GetPickRay((long)fMouseX, (long)fMouseY, master.masterrenderer);

	//pickRay.bIgnoreNearestTriangle = true; //PE: REMOVED - bIgnoreNearestTriangle no longer exists

	// check scene first if flagged, then terrain which is naturally underneath
	if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0 || (iLayerMask & GGRENDERLAYERS_CURSOROBJECT) != 0 || (iLayerMask & GGRENDERLAYERS_WIDGETPLANE) != 0)
	{
		wiScene::PickResult hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
		if (hovered.entity > 0)
		{
			if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0)
			{
				sObject* pHitObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(hovered.entity);
				if (pHitObject)
				{
					bool ObjectIsEntity(void* pTestObject);

					//PE: Only highlight if this is a gg entity.
					bool bIsEntity = ObjectIsEntity(pHitObject);
					if (bIsEntity)
					{
						if (iLayerMask & GGRENDERLAYERS_NORMAL)
						{
							g_hovered_pobject = pHitObject;
							g_hovered_entity = hovered.entity;
							g_hovered_dot_pobject = NULL;
							g_hovered_dot_entity = 0;
						}
					}
					else
					{
						//#define DOTARCSOBJECTID 70001+40000+21001
						if (pHitObject->dwObjectNumber > 110000 && pHitObject->dwObjectNumber < 131002)
						{
							if (iLayerMask & GGRENDERLAYERS_NORMAL)
							{
								g_hovered_dot_pobject = pHitObject;
								g_hovered_dot_entity = hovered.entity;
								g_bhovered_dot = true;
							}
						}
					}
				}
				else
				{
					if (iLayerMask & GGRENDERLAYERS_NORMAL)
					{
						g_hovered_dot_pobject = NULL;
						g_hovered_dot_entity = 0;
					}
				}
			}

			// return hit position
			*pOutX = hovered.position.x;
			*pOutY = hovered.position.y;
			*pOutZ = hovered.position.z;

			if (iLayerMask & GGRENDERLAYERS_NORMAL)
			{
				fLastHitPosition[0] = hovered.position.x;
				fLastHitPosition[1] = hovered.position.y;
				fLastHitPosition[2] = hovered.position.z;
			}

			// if normals needed
			if (pNormX)
			{
				*pNormX = hovered.normal.x;
				*pNormY = hovered.normal.y;
				*pNormZ = hovered.normal.z;
			}

			// optionally return actual object the cursor hovered over
			if (pHitEntity) *pHitEntity = hovered.entity;

			// report a hit
			bHitSuccess = true;
		}
	}
	if ((iLayerMask & GGRENDERLAYERS_TERRAIN) != 0)
	{
		float fDistToObjectHit = -1;
		if (bHitSuccess == true)
		{
			float fDX = *pOutX - CameraPositionX(0);
			float fDY = *pOutY - CameraPositionY(0);
			float fDZ = *pOutZ - CameraPositionZ(0);
			fDistToObjectHit = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
		}
		float pTerrOutX, pTerrOutY, pTerrOutZ, pTerrNormX, pTerrNormY, pTerrNormZ;
		if (GGTerrain::GGTerrain_RayCast(pickRay, &pTerrOutX, &pTerrOutY, &pTerrOutZ, &pTerrNormX, &pTerrNormY, &pTerrNormZ, 0))
		{
			fLastTerrainHitX = pTerrOutX, fLastTerrainHitY = pTerrOutY, fLastTerrainHitZ = pTerrOutZ;

			float fDX = pTerrOutX - CameraPositionX(0);
			float fDY = pTerrOutY - CameraPositionY(0);
			float fDZ = pTerrOutZ - CameraPositionZ(0);
			float fDist = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
			if (fDist < fDistToObjectHit || fDistToObjectHit == -1)
			{
				// if terrain closer than object hit, we register a terrain detection instead
				if (pHitEntity) *pHitEntity = 0;
				*pOutX = pTerrOutX;
				*pOutY = pTerrOutY;
				*pOutZ = pTerrOutZ;
				if (pNormX)
				{
					*pNormX = pTerrNormX;
					*pNormY = pTerrNormY;
					*pNormZ = pTerrNormZ;
				}
				bHitSuccess = true;
			}
		}
		else
		{
			fLastTerrainHitX = 0, fLastTerrainHitY = 0, fLastTerrainHitZ = 0;
		}
	}

	// no entity hovering over, but still want to move the cursor line in visual logic system
	if (g_hovered_entity == 0)
	{
		if (iLayerMask & GGRENDERLAYERS_NORMAL)
		{
			fLastHitPosition[0] = *pOutX;
			fLastHitPosition[1] = *pOutY;
			fLastHitPosition[2] = *pOutZ;
		}
	}
	// return success flag
	return bHitSuccess;
}

bool WickedCall_GetPick2_Thread(float fMouseX, float fMouseY, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask)
{
	// Use wicked mouse pointer to determine intersection with solid geometry (for terrain and entity detection)
	// PE: We got hits from hidden objects like "widgets" , now ignore hidden objects in WICKEDREPO.
	bool bHitSuccess = false;

	//PE: Do not do anything if mouse is not over level editor.
	//if (bImGuiGotFocus) return bHitSuccess;

	// do not wipe out hover object detection when widget plane pass happens
	if (iLayerMask & GGRENDERLAYERS_NORMAL)
	{
		g_hovered_pobject = NULL;
		g_hovered_entity = 0;
	}

	//PE: Wicked Mouse is relative to windows pos. ImGui is relative to screen.
	RAY pickRay = wiRenderer::GetPickRay((long)fMouseX, (long)fMouseY, master.masterrenderer);

	//pickRay.bIgnoreNearestTriangle = true; //PE: REMOVED - bIgnoreNearestTriangle no longer exists

	// check scene first if flagged, then terrain which is naturally underneath
	if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0 || (iLayerMask & GGRENDERLAYERS_CURSOROBJECT) != 0 || (iLayerMask & GGRENDERLAYERS_WIDGETPLANE) != 0)
	{
		wiScene::PickResult hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
		if (hovered.entity > 0)
		{
			if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0)
			{
				sObject* pHitObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(hovered.entity);
				if (pHitObject)
				{
					bool ObjectIsEntity(void* pTestObject);

					//PE: Only highlight if this is a gg entity.
					bool bIsEntity = ObjectIsEntity(pHitObject);
					if (bIsEntity)
					{
						if (iLayerMask & GGRENDERLAYERS_NORMAL)
						{
							g_hovered_pobject = pHitObject;
							g_hovered_entity = hovered.entity;
							g_hovered_dot_pobject = NULL;
							g_hovered_dot_entity = 0;
						}
					}
					else
					{
						//#define DOTARCSOBJECTID 70001+40000+21001
						if (pHitObject->dwObjectNumber > 110000 && pHitObject->dwObjectNumber < 131002)
						{
							if (iLayerMask & GGRENDERLAYERS_NORMAL)
							{
								g_hovered_dot_pobject = pHitObject;
								g_hovered_dot_entity = hovered.entity;
								g_bhovered_dot = true;
							}
						}
					}
				}
				else
				{
					if (iLayerMask & GGRENDERLAYERS_NORMAL)
					{
						g_hovered_dot_pobject = NULL;
						g_hovered_dot_entity = 0;
					}
				}
			}

			// return hit position
			*pOutX = hovered.position.x;
			*pOutY = hovered.position.y;
			*pOutZ = hovered.position.z;

			if (iLayerMask & GGRENDERLAYERS_NORMAL)
			{
				fLastHitPosition[0] = hovered.position.x;
				fLastHitPosition[1] = hovered.position.y;
				fLastHitPosition[2] = hovered.position.z;
			}

			// if normals needed
			if (pNormX)
			{
				*pNormX = hovered.normal.x;
				*pNormY = hovered.normal.y;
				*pNormZ = hovered.normal.z;
			}

			// optionally return actual object the cursor hovered over
			if (pHitEntity) *pHitEntity = hovered.entity;

			// report a hit
			bHitSuccess = true;
		}
	}
	if ((iLayerMask & GGRENDERLAYERS_TERRAIN) != 0)
	{
		float fDistToObjectHit = -1;
		if (bHitSuccess == true)
		{
			float fDX = *pOutX - CameraPositionX(0);
			float fDY = *pOutY - CameraPositionY(0);
			float fDZ = *pOutZ - CameraPositionZ(0);
			fDistToObjectHit = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
		}
		float pTerrOutX, pTerrOutY, pTerrOutZ, pTerrNormX, pTerrNormY, pTerrNormZ;
		if (GGTerrain::GGTerrain_RayCast(pickRay, &pTerrOutX, &pTerrOutY, &pTerrOutZ, &pTerrNormX, &pTerrNormY, &pTerrNormZ, 0))
		{
			fLastTerrainHitX = pTerrOutX, fLastTerrainHitY = pTerrOutY, fLastTerrainHitZ = pTerrOutZ;

			float fDX = pTerrOutX - CameraPositionX(0);
			float fDY = pTerrOutY - CameraPositionY(0);
			float fDZ = pTerrOutZ - CameraPositionZ(0);
			float fDist = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
			if (fDist < fDistToObjectHit || fDistToObjectHit == -1)
			{
				// if terrain closer than object hit, we register a terrain detection instead
				if (pHitEntity) *pHitEntity = 0;
				*pOutX = pTerrOutX;
				*pOutY = pTerrOutY;
				*pOutZ = pTerrOutZ;
				if (pNormX)
				{
					*pNormX = pTerrNormX;
					*pNormY = pTerrNormY;
					*pNormZ = pTerrNormZ;
				}
				bHitSuccess = true;
			}
		}
		else
		{
			fLastTerrainHitX = 0, fLastTerrainHitY = 0, fLastTerrainHitZ = 0;
		}
	}

	// no entity hovering over, but still want to move the cursor line in visual logic system
	if (g_hovered_entity == 0)
	{
		if (iLayerMask & GGRENDERLAYERS_NORMAL)
		{
			fLastHitPosition[0] = *pOutX;
			fLastHitPosition[1] = *pOutY;
			fLastHitPosition[2] = *pOutZ;
		}
	}

	// return success flag
	return bHitSuccess;
}
#endif

bool WickedCall_GetPick2(float fMouseX, float fMouseY, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask)
{
	// Use wicked mouse pointer to determine intersection with solid geometry (for terrain and entity detection)
	// PE: We got hits from hidden objects like "widgets" , now ignore hidden objects in WICKEDREPO.
	bool bHitSuccess = false;

	//PE: Do not do anything if mouse is not over level editor.
	if (bImGuiGotFocus) return bHitSuccess;

	// do not wipe out hover object detection when widget plane pass happens
	if (iLayerMask & GGRENDERLAYERS_NORMAL)
	{
		g_hovered_pobject = NULL;
		g_hovered_entity = 0;
	}

	//PE: Wicked Mouse is relative to windows pos. ImGui is relative to screen.
	RAY pickRay = wiRenderer::GetPickRay((long)fMouseX, (long)fMouseY, master.masterrenderer);

	//pickRay.bIgnoreNearestTriangle = true; //PE: REMOVED - bIgnoreNearestTriangle no longer exists
	
	// check scene first if flagged, then terrain which is naturally underneath
	if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0 || (iLayerMask & GGRENDERLAYERS_CURSOROBJECT) != 0 || (iLayerMask & GGRENDERLAYERS_WIDGETPLANE) != 0)
	{
#ifdef PICKBVHTHREADED
		wiScene::PickResult hovered;
			hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
#else
		wiScene::PickResult hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
#endif
		if (hovered.entity > 0)
		{
			if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0)
			{
				sObject* pHitObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(hovered.entity);
				if (pHitObject)
				{
					bool ObjectIsEntity(void* pTestObject);

					//PE: Only highlight if this is a gg entity.
					bool bIsEntity = ObjectIsEntity(pHitObject);
					if (bIsEntity)
					{
						if (iLayerMask & GGRENDERLAYERS_NORMAL)
						{
							g_hovered_pobject = pHitObject;
							g_hovered_entity = hovered.entity;
							g_hovered_dot_pobject = NULL;
							g_hovered_dot_entity = 0;
						}
					}
					else
					{
						//#define DOTARCSOBJECTID 70001+40000+21001
						if (pHitObject->dwObjectNumber > 110000 && pHitObject->dwObjectNumber < 131002)
						{
							if (iLayerMask & GGRENDERLAYERS_NORMAL)
							{
								g_hovered_dot_pobject = pHitObject;
								g_hovered_dot_entity = hovered.entity;
								g_bhovered_dot = true;
							}
						}
					}
				}
				else
				{
					if (iLayerMask & GGRENDERLAYERS_NORMAL)
					{
						g_hovered_dot_pobject = NULL;
						g_hovered_dot_entity = 0;
					}
				}
			}

			// return hit position
			*pOutX = hovered.position.x;
			*pOutY = hovered.position.y;
			*pOutZ = hovered.position.z;

			if (iLayerMask & GGRENDERLAYERS_NORMAL)
			{
				fLastHitPosition[0] = hovered.position.x;
				fLastHitPosition[1] = hovered.position.y;
				fLastHitPosition[2] = hovered.position.z;
			}

			// if normals needed
			if (pNormX)
			{
				*pNormX = hovered.normal.x;
				*pNormY = hovered.normal.y;
				*pNormZ = hovered.normal.z;
			}

			// optionally return actual object the cursor hovered over
			if (pHitEntity) *pHitEntity = hovered.entity;

			// report a hit
			bHitSuccess = true;
		}
	}
	if ((iLayerMask & GGRENDERLAYERS_TERRAIN) != 0)
	{
		float fDistToObjectHit = -1;
		if (bHitSuccess == true)
		{
			float fDX = *pOutX - CameraPositionX(0);
			float fDY = *pOutY - CameraPositionY(0);
			float fDZ = *pOutZ - CameraPositionZ(0);
			fDistToObjectHit = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
		}
		float pTerrOutX, pTerrOutY, pTerrOutZ, pTerrNormX, pTerrNormY, pTerrNormZ;
		if (GGTerrain::GGTerrain_RayCast(pickRay, &pTerrOutX, &pTerrOutY, &pTerrOutZ, &pTerrNormX, &pTerrNormY, &pTerrNormZ, 0))
		{
			fLastTerrainHitX = pTerrOutX, fLastTerrainHitY = pTerrOutY, fLastTerrainHitZ = pTerrOutZ;

			float fDX = pTerrOutX - CameraPositionX(0);
			float fDY = pTerrOutY - CameraPositionY(0);
			float fDZ = pTerrOutZ - CameraPositionZ(0);
			float fDist = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
			if (fDist < fDistToObjectHit || fDistToObjectHit == -1)
			{
				// if terrain closer than object hit, we register a terrain detection instead
				if (pHitEntity) *pHitEntity = 0;
				*pOutX = pTerrOutX;
				*pOutY = pTerrOutY;
				*pOutZ = pTerrOutZ;
				if (pNormX)
				{
					*pNormX = pTerrNormX;
					*pNormY = pTerrNormY;
					*pNormZ = pTerrNormZ;
				}
				bHitSuccess = true;
			}
		}
		else
		{
			fLastTerrainHitX = 0, fLastTerrainHitY = 0, fLastTerrainHitZ = 0;
		}
	}

	// no entity hovering over, but still want to move the cursor line in visual logic system
	if (g_hovered_entity == 0)
	{
		if (iLayerMask & GGRENDERLAYERS_NORMAL)
		{
			fLastHitPosition[0] = *pOutX;
			fLastHitPosition[1] = *pOutY;
			fLastHitPosition[2] = *pOutZ;
		}
	}
	// return success flag
	return bHitSuccess;
}


// GG reload hardening (2026-07-26, in-place-reload corruption fix): fully quiesce the GPU
// and flush the entire deferred-destroy backlog. Called from gridedit_load_map right after
// gridedit_clear_map: the old level just freed thousands of GPU resources while the
// renderer keeps presenting; letting their heap memory + bindless descriptor slots recycle
// lazily UNDER the new level's creation storm produced wrong-texture / garbage-vertex
// draws that worsened with every reload (blue palms, giant slabs — see the 2026-07-25
// forensic hunt). A cold load never corrupted because it creates into a clean device; this
// makes an in-place reload start from the same quiesced state. Cost: one GPU drain per
// level load (~ms — irrelevant next to the load itself).
#include "wiResourceManager.h" // GGMAX 1.44 reload guard

// GGMAX 3.35i: diagnostic files go beside the EXE, not into whatever the CWD currently is
// (a file dialog can move it). Defined in Guru-WickedMAX/master_part1.cpp.
extern FILE* GGDiagFopen(const char* name, const char* mode);

// GGMAX 2.71: false = quiesce runs identically but writes no reload_quiesce.txt (the
// unconditional append grew forever across sessions and landed in players' standalone
// folders; set via GGSetDiagTraceFiles from the producelogfiles setup.ini key)
bool gg_reload_quiesce_file = true;
// GG: the nearest surface of one object's own meshes along a ray (pDir of unit length), up to fLength, met from the ray's
// side as every pick takes it: true with its distance, normal and limb (frame). The model of a vehicle with no physics
// body (CollisionOff), which the physics ray can't meet; hidden limbs and an object made with no collision (mode 11) are
// never met
bool WickedCall_ObjectRay(sObject* pObject, const float* pFrom, const float* pDir, float fLength, float* pDistance, float* pNormal, int* pLimb)
{
	if (!pObject || !pObject->ppFrameList) return false;
	Scene& scene = wiScene::GetScene();
	static std::vector<uint32_t> targets;
	targets.clear();
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		sFrame* pFrame = pObject->ppFrameList[iF];
		if (!pFrame || pFrame->wickedobjindex == 0) continue;
		size_t i = scene.objects.GetIndex(pFrame->wickedobjindex);
		if (i < scene.objects.GetCount() && scene.objects.GetEntity(i) == pFrame->wickedobjindex) targets.push_back((uint32_t)i);
	}
	if (targets.empty()) return false;
	RAY pickRay(XMFLOAT3(pFrom[0], pFrom[1], pFrom[2]), XMFLOAT3(pDir[0], pDir[1], pDir[2]));
	pickRay.TMax = fLength;
	wiScene::PickResult hit = wiScene::PickObjects(pickRay, targets.data(), (uint32_t)targets.size(), RENDERTYPE_OPAQUE | RENDERTYPE_TRANSPARENT, ~0u);
	if (hit.entity == INVALID_ENTITY || hit.distance > fLength) return false;
	*pDistance = hit.distance;
	pNormal[0] = hit.normal.x; pNormal[1] = hit.normal.y; pNormal[2] = hit.normal.z;
	*pLimb = -1;
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		sFrame* pFrame = pObject->ppFrameList[iF];
		if (pFrame && pFrame->wickedobjindex == hit.entity) { *pLimb = iF; break; }
	}
	return true;
}


extern "C" void WickedCall_ReloadQuiesceGPU(void)
{
	// GGMAX 1.44 — THE ACTUAL FIX: quiesce the texture-streaming system before the reload
	// proceeds (join the in-flight streaming job, drop stale pending replacements, pause).
	// The streaming-off A/B (2026-07-26) proved streaming is the writer of the wrong
	// texture content; resumed in WickedCall_ReloadQuiesceEnd after the load completes.
	wi::resourcemanager::GGReloadGuardBegin();

	wiGraphics::GraphicsDevice* device = wiGraphics::GetDevice();
	if (device == nullptr) return;

	// GGMAX 1.43 DISABLED 2026-07-26 (suspected device-removal cause, do NOT re-enable
	// without fixing the frame-boundary problem): gridedit_load_map runs MID-FRAME while
	// the editor keeps pumping/rendering frames (Sync()/EmptyMessages inside the load).
	// Force-releasing the entire deferred-destroy backlog there can free resources that
	// recorded-but-unsubmitted command lists still reference -> GPU executes against dead
	// memory -> DXGI_ERROR_DEVICE_HUNG / "Device Lost on Present" (user-reported after this
	// delta shipped; harness repro then crashed 2/2 during load). It also never fixed any
	// corruption (proven: ~16.5k items drained per reload, artifacts unchanged).
	// The streaming guard above IS the useful part and stays.
	const bool gg_enable_deferred_flush = false;
	uint64_t released = gg_enable_deferred_flush ? device->FlushDeferredDestroys() : 0ull;
	// Diagnostic marker (corruption hunt): prove the hook ran and show how much it drained.
	static int s_quiesceCount = 0;
	s_quiesceCount++;
	FILE* f = gg_reload_quiesce_file ? GGDiagFopen("reload_quiesce.txt", "a") : nullptr;
	if (f != nullptr)
	{
		fprintf(f, "quiesce #%d: released %llu deferred GPU items (streaming guarded)\n", s_quiesceCount, (unsigned long long)released);
		fclose(f);
	}
}

// GGMAX 1.44: resume texture streaming after the level load completed (pairs with the
// GGReloadGuardBegin inside WickedCall_ReloadQuiesceGPU).
extern "C" void WickedCall_ReloadQuiesceEnd(void)
{
	wi::resourcemanager::GGReloadGuardEnd();
}

// Engine-native water ripple (replaces the DX11 additive ripple decal): renders into the
// water surface normal gradient — the same effect Jolt physics bodies spawn on splashdown.
// The engine default sprite is sized for meter worlds; MAX is inch-scaled, so callers pass
// a world-unit footprint (~40 = 1m; the sprite then grows and fades over ~1.7s).
void WickedCall_PutWaterRipple(float fX, float fY, float fZ, float fSize)
{
	wiScene::Scene& scene = wiScene::GetScene();
	scene.PutWaterRipple(XMFLOAT3(fX, fY, fZ));
	if (!scene.waterRipples.empty())
	{
		scene.waterRipples.back().params.siz = XMFLOAT2(fSize, fSize);
		// USER-TUNED 2026-07-28: half-strength normal perturbation (engine default 1.0
		// distorts too hard); opacity scales the additive write into the ripple buffer
		scene.waterRipples.back().params.opacity = 0.5f;
	}
}

// GGMAX 2.35: force the CACHED local (point/spot) shadow atlas to re-render next frame.
//
// Local shadow maps are cached (Phase 2) and only re-rendered when something is detected to have
// changed. That detector — GGMAX 2.07d, wiRenderer.cpp:5117 — finds a caster that MOVED, by
// comparing each object's world matrix against last frame's. It cannot possibly see a caster that
// was DELETED: a removed object is gone from the objects array, so there is no "now" matrix left
// to differ from its "prev" one, `changed` stays false, and the cached atlas keeps the deleted
// object's depth forever. That is the "delete the ammo in the editor and its shadow stays" bug,
// and the same hole covers collecting a pickup in-game.
// The engine already exports the nudge (wiRenderer.h:1200) — it simply had NO caller anywhere in
// the game, which is why nothing ever refreshed the cache on a content change. This is the whole
// fix; no engine change is involved.
void GGInvalidateLocalShadows()
{
	wi::renderer::InvalidateLocalShadows();
}
