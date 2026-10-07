#include "stdafx.h"
#include "WickedEngine.h"
#include "RippleManager.h"
#include "wiResourceManager.h"
#include "preprocessor-moreflags.h"
#include "gameguru.h"

std::shared_ptr<wiResource> WickedCall_LoadImage(std::string pFilenameToLoad);

using namespace wiGraphics;

// GG: rain ripples drawn as one batch (SetRippleBatching); see RippleManager.h
namespace Ripples
{
	static const uint32_t RIPPLES_MAX = 2048;

	struct Ripple
	{
		XMFLOAT3 pos;
		float sizeX;
		XMFLOAT3 normal;
		float sizeY;
		float age;
		float life;
		int frameStart;
		int frameEnd;
	};

	// one ring for the vertex shader (RainRippleVS.hlsl)
	struct RippleGPU
	{
		XMFLOAT4 posSizeX;
		XMFLOAT4 normalSizeY;
		XMFLOAT4 anim; // x its age in seconds, y its first frame, z the frame it ends before
	};

	struct RippleCB
	{
		XMMATRIX viewProj;
		XMFLOAT4 camPos;
		XMFLOAT4 anim; // x frames a second, y frames across, z frames down
		XMFLOAT4 fog; // x where the fog starts, y where it is full
	};

	bool bEnabled = false;
	bool bInitialised = false;
	std::vector<Ripple> ripples;
	std::vector<RippleGPU> gpuRipples;
	int iDropped = 0;

	std::string imageFile;
	std::shared_ptr<wiResource> image;
	int iAcross = 1;
	int iDown = 1;
	float fFramesPerSecond = 40.0f;

	GPUBuffer instanceBuffer;
	GPUBuffer constantBuffer;
	Shader shaderVS;
	Shader shaderPS;
	BlendState blendAdditive;
	RasterizerState rasterizer;
	DepthStencilState depthStencil;
	Sampler sampler;
	PipelineState pso;

	void Initialize()
	{
		if (bInitialised) return;
		bInitialised = true;
		GraphicsDevice* device = wiRenderer::GetDevice();
		ripples.reserve(RIPPLES_MAX);
		gpuRipples.resize(RIPPLES_MAX);

		// made once here, never during play
		GPUBufferDesc bd;
		bd.Usage = USAGE_DYNAMIC;
		bd.CPUAccessFlags = CPU_ACCESS_WRITE;
		bd.BindFlags = BIND_SHADER_RESOURCE;
		bd.MiscFlags = RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = sizeof(RippleGPU);
		bd.ByteWidth = sizeof(RippleGPU) * RIPPLES_MAX;
		device->CreateBuffer(&bd, nullptr, &instanceBuffer);

		GPUBufferDesc cbd;
		cbd.Usage = USAGE_DYNAMIC;
		cbd.CPUAccessFlags = CPU_ACCESS_WRITE;
		cbd.BindFlags = BIND_CONSTANT_BUFFER;
		cbd.ByteWidth = sizeof(RippleCB);
		device->CreateBuffer(&cbd, nullptr, &constantBuffer);

		// additive, as the ripple decal's material was (g_iBlendMode 5)
		blendAdditive.RenderTarget[0].BlendEnable = true;
		blendAdditive.RenderTarget[0].SrcBlend = BLEND_SRC_ALPHA;
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

		SamplerDesc sd;
		sd.Filter = FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = TEXTURE_ADDRESS_CLAMP;
		sd.AddressV = TEXTURE_ADDRESS_CLAMP;
		sd.AddressW = TEXTURE_ADDRESS_CLAMP;
		device->CreateSampler(&sd, &sampler);

		wiRenderer::LoadShader(VS, shaderVS, "RainRippleVS.cso");
		wiRenderer::LoadShader(PS, shaderPS, "RainRipplePS.cso");
		PipelineStateDesc desc;
		desc.vs = &shaderVS;
		desc.ps = &shaderPS;
		desc.bs = &blendAdditive;
		desc.rs = &rasterizer;
		desc.dss = &depthStencil;
		desc.pt = TRIANGLELIST;
		device->CreatePipelineState(&desc, &pso);
	}

	void SetEnabled(bool bOn)
	{
		bEnabled = bOn;
		if (!bEnabled) Clear();
	}

	bool IsEnabled()
	{
		return bEnabled;
	}

	void SetLook(const char* pImageFile, int across, int down, float framesPerSecond)
	{
		if (pImageFile && imageFile != pImageFile)
		{
			imageFile = pImageFile;
			image = WickedCall_LoadImage(imageFile);
		}
		iAcross = across < 1 ? 1 : across;
		iDown = down < 1 ? 1 : down;
		fFramesPerSecond = framesPerSecond > 0.001f ? framesPerSecond : 0.001f;
	}

	void Add(float fX, float fY, float fZ, float fSizeX, float fSizeY, float fNX, float fNY, float fNZ, int iFrameStart, int iFrameEnd)
	{
		if (ripples.size() >= RIPPLES_MAX) { iDropped++; return; }
		Ripple r;
		r.pos = XMFLOAT3(fX, fY, fZ);
		r.sizeX = fSizeX;
		r.sizeY = fSizeY;
		float fLength = sqrtf(fNX * fNX + fNY * fNY + fNZ * fNZ);
		r.normal = fLength > 0.001f ? XMFLOAT3(fNX / fLength, fNY / fLength, fNZ / fLength) : XMFLOAT3(0, 1, 0);
		r.age = 0;
		r.frameStart = iFrameStart;
		r.frameEnd = iFrameEnd;
		// as the decal: it ends once its frame passes its last but one
		r.life = (float)(iFrameEnd - 1 - iFrameStart) / fFramesPerSecond;
		ripples.push_back(r);
	}

	void Update(float fSeconds)
	{
		for (size_t i = 0; i < ripples.size(); )
		{
			ripples[i].age += fSeconds;
			if (ripples[i].age > ripples[i].life)
			{
				ripples[i] = ripples.back();
				ripples.pop_back();
			}
			else
			{
				i++;
			}
		}
	}

	void Clear()
	{
		ripples.clear();
		iDropped = 0;
	}

	void GetStats(int* piLive, int* piCapacity, int* piDropped)
	{
		*piLive = (int)ripples.size();
		*piCapacity = (int)RIPPLES_MAX;
		*piDropped = iDropped;
	}

	extern "C" void ripple_draw(const wiScene::CameraComponent& camera, CommandList cmd)
	{
		if (!bInitialised || ripples.empty() || !image || !pso.IsValid()) return;
		GraphicsDevice* device = wiRenderer::GetDevice();
		device->EventBegin("Rain Ripples", cmd);

		const uint32_t count = (uint32_t)ripples.size();
		for (uint32_t i = 0; i < count; i++)
		{
			const Ripple& r = ripples[i];
			gpuRipples[i].posSizeX = XMFLOAT4(r.pos.x, r.pos.y, r.pos.z, r.sizeX);
			gpuRipples[i].normalSizeY = XMFLOAT4(r.normal.x, r.normal.y, r.normal.z, r.sizeY);
			gpuRipples[i].anim = XMFLOAT4(r.age, (float)r.frameStart, (float)r.frameEnd, 0);
		}
		device->UpdateBuffer(&instanceBuffer, gpuRipples.data(), cmd, (int)(sizeof(RippleGPU) * count));

		RippleCB cb;
		cb.viewProj = XMMatrixTranspose(camera.GetViewProjection());
		cb.camPos = XMFLOAT4(camera.Eye.x, camera.Eye.y, camera.Eye.z, 0);
		cb.anim = XMFLOAT4(fFramesPerSecond, (float)iAcross, (float)iDown, 0);
		const wiScene::WeatherComponent& weather = wiScene::GetScene().weather;
		cb.fog = XMFLOAT4(weather.fogStart, weather.fogEnd, 0, 0);
		device->UpdateBuffer(&constantBuffer, &cb, cmd);

		device->BindPipelineState(&pso, cmd);
		device->BindConstantBuffer(VS, &constantBuffer, 2, cmd);
		device->BindConstantBuffer(PS, &constantBuffer, 2, cmd);
		device->BindResource(VS, &instanceBuffer, 1, cmd);
		device->BindResource(PS, &image->texture, 0, cmd);
		device->BindSampler(PS, &sampler, 0, cmd);
		device->DrawInstanced(6, count, 0, 0, cmd);

		wiRenderer::BindCommonResources(cmd);
		device->EventEnd(cmd);
	}
}
