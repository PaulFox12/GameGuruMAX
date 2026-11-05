#include "stdafx.h"
#include "WickedEngine.h"
#include "DistantLightManager.h"
#include "wiResourceManager.h"
#include "preprocessor-moreflags.h"
#include "gameguru.h"

using namespace wiGraphics;

extern bool g_bNoTerrainRender;

// GG: distant lights (#220); see DistantLightManager.h
namespace DistantLights
{
	static const uint32_t DISTANTLIGHTS_MAX = 65536;

	struct Light
	{
		XMFLOAT4 posSize;
		XMFLOAT4 color;
		int tag;
	};

	// one light for the vertex shader (DistantLightVS.hlsl)
	struct LightGPU
	{
		XMFLOAT4 posSize;
		XMFLOAT4 color;
	};

	struct DistantLightCB
	{
		XMMATRIX viewProj;
		XMFLOAT4 camPos;
		XMFLOAT4 screen;
		XMFLOAT4 fade;
		XMFLOAT4 fog;
		XMFLOAT4 water;
	};

	bool bInitialised = false;
	std::vector<Light> lights;
	std::vector<LightGPU> gpuLights;
	bool bTagOn[256];
	bool bDirty = true;
	uint32_t iDrawn = 0;
	float fAmount = 1.0f;
	float fMinPixels = 1.5f;
	float fMaxPixels = 3.0f;
	float fFadeFrom = 6000.0f;
	float fFadeTo = 12000.0f;
	float fPull = 60.0f;

	GPUBuffer lightBuffer;
	GPUBuffer constantBuffer;
	Shader shaderVS;
	Shader shaderPS;
	BlendState blendAdditive;
	RasterizerState rasterizer;
	DepthStencilState depthStencil;
	PipelineState pso;

	void Initialize()
	{
		if (bInitialised) return;
		bInitialised = true;
		for (int t = 0; t < 256; t++) bTagOn[t] = true;
		GraphicsDevice* device = wiRenderer::GetDevice();

		// made once here, written when the list changes
		GPUBufferDesc bd;
		bd.Usage = USAGE_DYNAMIC;
		bd.CPUAccessFlags = CPU_ACCESS_WRITE;
		bd.BindFlags = BIND_SHADER_RESOURCE;
		bd.MiscFlags = RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = sizeof(LightGPU);
		bd.ByteWidth = sizeof(LightGPU) * DISTANTLIGHTS_MAX;
		device->CreateBuffer(&bd, nullptr, &lightBuffer);

		GPUBufferDesc cbd;
		cbd.Usage = USAGE_DYNAMIC;
		cbd.CPUAccessFlags = CPU_ACCESS_WRITE;
		cbd.BindFlags = BIND_CONSTANT_BUFFER;
		cbd.ByteWidth = sizeof(DistantLightCB);
		device->CreateBuffer(&cbd, nullptr, &constantBuffer);

		// added to the scene, its alpha left alone
		blendAdditive.RenderTarget[0].BlendEnable = true;
		blendAdditive.RenderTarget[0].SrcBlend = BLEND_ONE;
		blendAdditive.RenderTarget[0].DestBlend = BLEND_ONE;
		blendAdditive.RenderTarget[0].BlendOp = BLEND_OP_ADD;
		blendAdditive.RenderTarget[0].SrcBlendAlpha = BLEND_ZERO;
		blendAdditive.RenderTarget[0].DestBlendAlpha = BLEND_ONE;
		blendAdditive.RenderTarget[0].BlendOpAlpha = BLEND_OP_ADD;
		blendAdditive.RenderTarget[0].RenderTargetWriteMask = COLOR_WRITE_ENABLE_ALL;
		blendAdditive.IndependentBlendEnable = false;

		rasterizer.FillMode = FILL_SOLID;
		rasterizer.CullMode = CULL_NONE;
		rasterizer.FrontCounterClockwise = true;
		rasterizer.DepthClipEnable = true;

		// tested against the scene, not written
		depthStencil.DepthEnable = true;
		depthStencil.DepthWriteMask = DEPTH_WRITE_MASK_ZERO;
		depthStencil.DepthFunc = COMPARISON_GREATER_EQUAL;
		depthStencil.StencilEnable = false;

		wiRenderer::LoadShader(VS, shaderVS, "DistantLightVS.cso");
		wiRenderer::LoadShader(PS, shaderPS, "DistantLightPS.cso");
		PipelineStateDesc desc;
		desc.vs = &shaderVS;
		desc.ps = &shaderPS;
		desc.bs = &blendAdditive;
		desc.rs = &rasterizer;
		desc.dss = &depthStencil;
		desc.pt = TRIANGLELIST;
		device->CreatePipelineState(&desc, &pso);
	}

	bool Add(float fX, float fY, float fZ, float fR, float fG, float fB, float fSize, float fBrightness, int iTag)
	{
		if (lights.size() >= DISTANTLIGHTS_MAX) return false;
		Light l;
		l.posSize = XMFLOAT4(fX, fY, fZ, fSize > 0 ? fSize : 0);
		l.color = XMFLOAT4(fR, fG, fB, fBrightness > 0 ? fBrightness : 0);
		l.tag = iTag < 0 ? 0 : (iTag > 255 ? 255 : iTag);
		lights.push_back(l);
		bDirty = true;
		return true;
	}

	void Clear(int iTag)
	{
		if (iTag < 0)
		{
			lights.clear();
			for (int t = 0; t < 256; t++) bTagOn[t] = true;
		}
		else
		{
			lights.erase(std::remove_if(lights.begin(), lights.end(), [iTag](const Light& l) { return l.tag == iTag; }), lights.end());
		}
		bDirty = true;
	}

	void SetAmount(float fNewAmount) { fAmount = fNewAmount > 0 ? fNewAmount : 0; }
	float GetAmount() { return fAmount; }

	void SetTag(int iTag, bool bOn)
	{
		if (iTag < 0 || iTag > 255 || bTagOn[iTag] == bOn) return;
		bTagOn[iTag] = bOn;
		bDirty = true;
	}

	void SetLook(float fNewMinPixels, float fNewMaxPixels, float fNewFadeFrom, float fNewFadeTo, float fNewPull)
	{
		fMinPixels = fNewMinPixels > 0.5f ? fNewMinPixels : 0.5f;
		fMaxPixels = fNewMaxPixels > fMinPixels ? fNewMaxPixels : fMinPixels;
		fFadeFrom = fNewFadeFrom > 0 ? fNewFadeFrom : 0;
		fFadeTo = fNewFadeTo > fFadeFrom ? fNewFadeTo : fFadeFrom;
		fPull = fNewPull > 0 ? fNewPull : 0;
	}

	void GetLook(float* pMinPixels, float* pMaxPixels, float* pFadeFrom, float* pFadeTo, float* pPull)
	{
		*pMinPixels = fMinPixels;
		*pMaxPixels = fMaxPixels;
		*pFadeFrom = fFadeFrom;
		*pFadeTo = fFadeTo;
		*pPull = fPull;
	}

	void GetStats(int* piCount, int* piDrawn, int* piCapacity)
	{
		if (bDirty)
		{
			int iOn = 0;
			for (const Light& l : lights) if (bTagOn[l.tag]) iOn++;
			*piDrawn = iOn;
		}
		else
		{
			*piDrawn = (int)iDrawn;
		}
		*piCount = (int)lights.size();
		*piCapacity = (int)DISTANTLIGHTS_MAX;
	}

	extern "C" void distantlight_draw(const wiScene::CameraComponent& camera, CommandList cmd)
	{
		if (!bInitialised || lights.empty() || fAmount <= 0 || !pso.IsValid()) return;
		GraphicsDevice* device = wiRenderer::GetDevice();

		// the lights whose tag is on, written only when the list or a tag changed
		if (bDirty)
		{
			gpuLights.clear();
			for (const Light& l : lights)
			{
				if (!bTagOn[l.tag]) continue;
				LightGPU g;
				g.posSize = l.posSize;
				g.color = l.color;
				gpuLights.push_back(g);
			}
			iDrawn = (uint32_t)gpuLights.size();
			if (iDrawn > 0) device->UpdateBuffer(&lightBuffer, gpuLights.data(), cmd, (int)(sizeof(LightGPU) * iDrawn));
			bDirty = false;
		}
		if (iDrawn == 0) return;
		device->EventBegin("Distant Lights", cmd);

		DistantLightCB cb;
		cb.viewProj = XMMatrixTranspose(camera.GetViewProjection());
		XMFLOAT4X4 projection;
		XMStoreFloat4x4(&projection, camera.GetProjection());
		cb.camPos = XMFLOAT4(camera.Eye.x, camera.Eye.y, camera.Eye.z, projection._22);
		cb.screen = XMFLOAT4(camera.width, camera.height, fMinPixels, fMaxPixels);
		cb.fade = XMFLOAT4(fFadeFrom, fFadeTo, fAmount, fPull);
		// the scene's fog as wiRenderer gives it to objectHF.hlsli ApplyFog, of which a light takes the level's fog opacity's
		// share as a surface does without the realistic sky; with it a surface is fogged whole, but a light still shows
		// through the haze, so the opacity's share there too
		const wiScene::WeatherComponent& weather = wiScene::GetScene().weather;
		float fFogStart = g_bNoTerrainRender ? 900000.0f : weather.fogStart;
		float fFogEnd = g_bNoTerrainRender ? 1000000.0f : weather.fogEnd;
		float fFogShare = std::min(1.0f, std::max(0.0f, weather.fogColorAndOpacity.w));
		cb.fog = XMFLOAT4(fFogStart, fFogEnd > fFogStart ? 4.0f / (fFogEnd - fFogStart) : 0.0f, fFogShare, 0);
		const auto& ocean = weather.oceanParameters;
		float fWaterRate = ocean.fogMaxDist > ocean.fogMinDist ? 4.0f / (ocean.fogMaxDist - ocean.fogMinDist) : 0.0f;
		float fWaterMost = std::min(1.0f, std::max(0.0f, 1.0f - ocean.fogMinAmount));
		cb.water = XMFLOAT4(weather.IsOceanEnabled() ? ocean.waterHeight : -1.0e30f, ocean.fogMinDist, fWaterRate, fWaterMost);
		device->UpdateBuffer(&constantBuffer, &cb, cmd);

		device->BindPipelineState(&pso, cmd);
		device->BindConstantBuffer(VS, &constantBuffer, 2, cmd);
		device->BindResource(VS, &lightBuffer, 1, cmd);
		device->DrawInstanced(6, iDrawn, 0, 0, cmd);

		wiRenderer::BindCommonResources(cmd);
		device->EventEnd(cmd);
	}
}
