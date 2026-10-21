//#pragma optimize("", off)

//#define DISABLETEMP

// Force update
#include "stdafx.h"
#include "../../../../WickedEngineDX12/WickedEngine/WickedEngine.h"
#include "TracerManager.h"
#include "wiResourceManager.h"
#include "wiTimer.h"
#include "preprocessor-moreflags.h"
#include "gameguru.h"
// GGMAX 3.55: DDS loading is delegated to the GGTerrain TU rather than parsing it here.
// Utility/dds.h CANNOT be included from this file: gameguru.h drags in the DarkBASIC SDK's
// ddraw.h, whose DDSD_* / DDPF_* / DDSCAPS_* PREPROCESSOR MACROS expand inside dds.h's own
// enumerator lists and shred them (46 errors, "syntax error: constant" at dds.h:96).
// GGTerrain_part0.cpp has no gameguru.h, which is why the same header parses cleanly there.
namespace GGTerrain { void GGTerrain_LoadTextureDDS( const char* filename, wi::graphics::Texture* tex ); }
#ifdef OPTICK_ENABLE
#include "optick.h"
#endif


template<typename T> static inline T PELerp(T a, T b, float t) { return (T)(a + (b - a) * t); }

//PE: Tracers will now follow gunid (400) Added 100 for LUA only use.
#define MAXTRACERS 400
#define MAXLUATRACERS 100
// GGMAX 3.55: bounded-buffer backstop for AddTracer - see the comment there.
#define GG_TRACER_HARDCAP 4096
// GGMAX 3.56 (Lee, 2026-09-19): ownerless-tracer cull. A tracer quad is LONG, so one fired
// from BEHIND the camera renders over your shoulder, past you, and terminates a few hundred
// units into the scene - a streak with no visible owner. Present in DX11 too. Suppress a
// tracer whose ORIGIN (the shooter) is outside this half-angle from the camera forward axis.
//   1.0 = dead ahead only, 0.0 = cull only what is strictly behind (90 deg), -1.0 = never cull.
// 0.0 is deliberate: it kills the over-the-shoulder case Lee described without also killing
// the legitimate flanker whose streak crosses your view from just off-screen. Raise toward
// cos(horizontal FOV/2) (~0.5-0.6) to tighten it to "shooter must be on screen".
#define GG_TRACER_MIN_ORIGIN_COS 0.0f
// ...but NEVER cull a tracer originating this close to the camera. The PLAYER's own muzzle is
// the camera itself, nudged ~30 units by iTracerPosition (G-Gun_part2.cpp:1113-1120), so its
// forward component can be zero or negative. Without this exemption the cull above would
// delete your own weapon's tracers.
#define GG_TRACER_OWNCAM_RADIUS 120.0f

namespace Tracers
{
    std::vector<Tracer> tracers;
    GPUBuffer constantBuffer;
    Texture tracerTexture[MAXTRACERS + MAXLUATRACERS];
    PipelineState tracerPSO;
    bool tracerSystemReady = false;
    GPUBuffer quadVB;
    GPUBuffer quadIB;


    float gameTime = 0.0f;
    wiTimer mytimer;
    Sampler samplerTrilinearWrap;
    Sampler samplerTrilinearClamp;
    BlendState blendStatesAdditive;
    Shader shaderTracerVS;
    Shader shaderTracerPS;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    // GGMAX 3.55: MUST be namespace scope, not a CreatePipelineState local. PipelineStateDesc
    // stores a POINTER to it and DX12 defers the real pipeline compile to the first bind, by
    // which time a stack local's SemanticName strings are freed. Same trap as GGTerrainBake 2.96.
    InputLayout tracerInputLayout;
    // GGMAX 3.55: instrumentation for DUMP_TRACERS - lets a "no tracers on screen" result name
    // its own cause (not spawned / no texture / not drawn) instead of needing another hunt.
    int  g_ggDrawnLastFrame = 0;
    int  g_ggSpawnedTotal   = 0;
    int  g_ggDrawCalls      = 0;
    int  g_ggCulledBehind   = 0;

    struct Vertex { XMFLOAT3 pos; XMFLOAT2 uv; };
    // GG: centred across (x -0.5 to 0.5), so the streak's core runs along the shot line; from 0 to 1 it lay width/2 to
    // one side, the side turning with the camera
    Vertex vertices[] = {
        { XMFLOAT3(-0.5f, -0.5f, 0), XMFLOAT2(0, 1) },
        { XMFLOAT3( 0.5f, -0.5f, 0), XMFLOAT2(1, 1) },
        { XMFLOAT3(-0.5f,  0.5f, 0), XMFLOAT2(0, 0) },
        { XMFLOAT3( 0.5f,  0.5f, 0), XMFLOAT2(1, 0) },
    };
    uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };

    // GGMAX 3.55: the Feb port (d3ae5996, "741 -> 0 errors") deleted tinyddsloader and left
    // this an EMPTY STUB, so every tracerTexture[] slot has been invalid ever since - and with
    // the draw hook also missing, nobody noticed for seven months. Now forwards to the shipping
    // GGTerrain_LoadTextureDDS (GGTerrain_part0.cpp:4397), which already handles the exact
    // format these assets use (32x512 BC3, 10 mips, legacy FourCC) and is the same loader the
    // terrain has used all along. One DDS path, not two.
    void Tracer_LoadTextureDDS(const char* filename, Texture* tex)
    {
        GGTerrain::GGTerrain_LoadTextureDDS(filename, tex);
    }

    void LoadTracerImage(char * filename , uint32_t gunid)
    {
        // GGMAX 3.55: gunid is bounded by g.maxgunsinengine (1000, or 400 under REDUCEMEMUSE),
        // but tracerTexture[] is only MAXTRACERS+MAXLUATRACERS = 500. Harmless while the loader
        // was a stub; an out-of-bounds WRITE the moment it does something. Pre-existing in DX11.
        if (gunid >= MAXTRACERS + MAXLUATRACERS) return;
        Tracer_LoadTextureDDS(filename, &tracerTexture[gunid]);
    }
    
    void Initialize()
    {
#ifdef DISABLETEMP
        return;
#endif
        GraphicsDevice* device = wi::graphics::GetDevice();
        //PE: Load tracer texture
        //PE: Moved to gunfolder/tracer.dds
        //Tracer_LoadTextureDDS("files/gamecore/tracers/tracer1.dds", &tracerTexture[0]);
        //Tracer_LoadTextureDDS("files/gamecore/tracers/tracer2.dds", &tracerTexture[1]);

        //PE: Create constant buffer
        GPUBufferDesc cbd;
        cbd.usage = Usage::UPLOAD;
        cbd.size = sizeof(TracerCB);
        cbd.bind_flags = BindFlag::CONSTANT_BUFFER;
        //cbd.CPUAccessFlags = CPU_ACCESS_WRITE; // removed in DX12 API
        device->CreateBuffer(&cbd, nullptr, &constantBuffer);

        SamplerDesc samplerDesc;
        samplerDesc.address_u = TextureAddressMode::WRAP;
        samplerDesc.address_v = TextureAddressMode::WRAP;
        samplerDesc.address_w = TextureAddressMode::WRAP;
        // GG: anisotropic, the one the streaks are drawn with: seen from low behind a streak's length is squashed into a few
        // pixels, and trilinear took its mip from that, so the narrow core blurred across the whole width and the streak
        // drew as a solid bar; anisotropic takes the mip from the width and samples along the length
        samplerDesc.filter = Filter::ANISOTROPIC;
        samplerDesc.max_anisotropy = 16;
        device->CreateSampler(&samplerDesc, &samplerTrilinearWrap);
        samplerDesc.max_anisotropy = 0;

        samplerDesc.address_u = TextureAddressMode::CLAMP;
        samplerDesc.address_v = TextureAddressMode::CLAMP;
        samplerDesc.address_w = TextureAddressMode::CLAMP;
        samplerDesc.filter = Filter::MIN_MAG_MIP_LINEAR;
        device->CreateSampler(&samplerDesc, &samplerTrilinearClamp);

        BlendState bd;
        //PE: Additive for now.
        bd.render_target[0].blend_enable = true;
        bd.render_target[0].src_blend = Blend::SRC_ALPHA;
        bd.render_target[0].dest_blend = Blend::ONE;
        bd.render_target[0].blend_op = BlendOp::ADD;
        bd.render_target[0].src_blend_alpha = Blend::ZERO;
        bd.render_target[0].dest_blend_alpha = Blend::ONE;
        bd.render_target[0].blend_op_alpha = BlendOp::ADD;
        bd.render_target[0].render_target_write_mask = ColorWrite::ENABLE_ALL;
        bd.independent_blend_enable = false;
        blendStatesAdditive = bd;

        /*
        //Alpha
        bd.RenderTarget[0].BlendEnable = true;
        bd.RenderTarget[0].SrcBlend = BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = COLOR_WRITE_ENABLE_ALL;
        bd.independent_blend_enable = false;
        blendStatesAdditive = bd;
        */

        RasterizerState rs;
        rs.fill_mode = FillMode::SOLID;
        rs.cull_mode = CullMode::NONE;
        rs.front_counter_clockwise = true;
        rs.depth_bias = 0;
        rs.depth_bias_clamp = 0;
        rs.slope_scaled_depth_bias = 0;
        rs.depth_clip_enable = false;
        rs.multisample_enable = false;
        rs.antialiased_line_enable = false;
        rasterizerState = rs;

        DepthStencilState dsd;
        dsd.depth_enable = true;
        dsd.depth_write_mask = DepthWriteMask::ZERO;
        dsd.depth_func = ComparisonFunc::GREATER_EQUAL;
        dsd.stencil_enable = false;
        depthStencilState = dsd;

        //PE: Create quad mesh and pipeline state
        CreateQuad();
        CreatePipelineState();
    }

    void AddTracer(const XMFLOAT3& start, const XMFLOAT3& end, float lifeTime, XMFLOAT4 color, float glow, float scroll, float scaleV, float width, float max_length, uint32_t tracerID)
    {
#ifdef DISABLETEMP
        return;
#endif
        // GGMAX 3.55: Update() - which ages tracers out and advances gameTime - runs only from
        // tracer_draw. While the draw hook was missing this was an UNCAPPED push_back: every NPC
        // shot leaked an entry for the whole session. The hook is restored, so this cap should
        // never be reached; it is here so a future unhooking degrades to a bounded buffer instead
        // of unbounded growth. Live count at 0.15 s lifetime is single digits.
        if (tracers.size() >= GG_TRACER_HARDCAP) return;
        g_ggSpawnedTotal++;
        tracers.push_back({ start, end, gameTime, lifeTime, color, glow, scroll ,scaleV, width, max_length, tracerID });
    }

    // GGMAX 3.55: drop every live tracer at a level swap. Without this, streaks spawned in the
    // outgoing level survive into the incoming one and draw for their remaining lifetime.
    void ClearLevel()
    {
        tracers.clear();
    }

    // GGMAX 3.55: one-line status for the DUMP_TRACERS harness verb.
    void DebugStatus(char* out, int osize)
    {
        int valid = 0;
        for (int i = 0; i < MAXTRACERS + MAXLUATRACERS; i++) if (tracerTexture[i].IsValid()) valid++;
        _snprintf(out, osize,
            "ready=%d live=%d spawned_total=%d drawn_lastframe=%d drawpasses=%d texslots_valid=%d gametime=%.2f culled_behind=%d",
            tracerSystemReady ? 1 : 0, (int)tracers.size(), g_ggSpawnedTotal,
            g_ggDrawnLastFrame, g_ggDrawCalls, valid, gameTime, g_ggCulledBehind);
        out[osize - 1] = 0;
    }


    void Update()
    {
#ifdef DISABLETEMP
        return;
#endif
        float currentTime = gameTime;

        tracers.erase(std::remove_if(tracers.begin(), tracers.end(), [currentTime](const Tracer& tracer)
            {
                return (currentTime - tracer.spawnTime) > tracer.lifeTime;
            }), tracers.end());

        float deltaTime = float(std::max(0.0, mytimer.elapsed() / 1000.0));
        if (deltaTime > (1.0f / 20.0f)) deltaTime = (1.0f / 20.0f);
        mytimer.record();

        gameTime += deltaTime;
    }


    extern "C" void tracer_draw(const wiScene::CameraComponent& camera, wiGraphics::CommandList cmd)
    {
#ifdef OPTICK_ENABLE
        OPTICK_EVENT();
#endif

#ifdef DISABLETEMP
        return;
#endif
        Draw(cmd, camera.GetViewProjection());
        Tracers::Update();
    }

    void Draw(CommandList cmd, const XMMATRIX& viewProj)
    {
#ifdef DISABLETEMP
        return;
#endif
        if (!tracerSystemReady) return;
        if (tracers.empty()) return;

        wiRenderer::BindCommonResources(cmd);

        wiScene::CameraComponent& camera = wiScene::GetCamera();
        const XMMATRIX myViewProj = camera.GetViewProjection();
        
        GraphicsDevice* device = wi::graphics::GetDevice();
        g_ggDrawnLastFrame = (int)tracers.size();
        g_ggDrawCalls++;
        device->EventBegin("tracer Draw", cmd);
        device->BindPipelineState(&tracerPSO, cmd);

        // GGMAX 3.56: camera basis for the ownerless-tracer cull, hoisted out of the loop.
        // camera.At is a NORMALIZED forward direction in this engine, not a look-at point
        // (wiScene_Components.cpp:2771 feeds it to XMMatrixLookToLH).
        const XMVECTOR ggCamEye = XMLoadFloat3(&camera.Eye);
        const XMVECTOR ggCamFwd = XMVector3Normalize(XMLoadFloat3(&camera.At));

        for (const auto& tracer : tracers)
        {
            XMVECTOR start = XMLoadFloat3(&tracer.startPos);
            XMVECTOR end = XMLoadFloat3(&tracer.endPos);

            // GGMAX 3.56: skip a tracer whose SHOOTER is behind/beside the camera - see the
            // GG_TRACER_MIN_ORIGIN_COS note. Drawn-time, not spawn-time, so the decision stays
            // correct while the camera turns during the tracer's life. The tracer still ages
            // out normally; only its draw is suppressed.
            {
                XMVECTOR ggToOrigin = XMVectorSubtract(start, ggCamEye);
                float ggOriginDist = XMVectorGetX(XMVector3Length(ggToOrigin));
                if (ggOriginDist > GG_TRACER_OWNCAM_RADIUS)
                {
                    float ggCos = XMVectorGetX(XMVector3Dot(XMVectorScale(ggToOrigin, 1.0f / ggOriginDist), ggCamFwd));
                    if (ggCos < GG_TRACER_MIN_ORIGIN_COS) { g_ggCulledBehind++; continue; }
                }
            }
            XMVECTOR dir = XMVectorSubtract(end, start);
            float length = XMVectorGetX(XMVector3Length(dir)); //Hit weapon ? *0.97;
            dir = XMVector3Normalize(dir);

            const float width = tracer.width;

            // GG: from the spawn time itself; a frame's elapsed time taken off it held every streak at its start for a second
            // drawn frame (gameTime moves on after Draw), a frame behind a moving gun's muzzle, and it faded to 0 only after
            // Update had removed it
            float lifeFraction = (gameTime - tracer.spawnTime) / tracer.lifeTime;
            float alpha = std::clamp(1.0f - lifeFraction,0.0f, 1.0f);

            float max_width = tracer.max_length; // 50
            if (max_width > 0)
            {
                length = max_width;
            }

            //PE: Move middle point to start position using length.
            XMVECTOR centre;
            if (max_width > 0)
            {
                XMVECTOR newstart = XMVectorLerp(start, end - (dir * (max_width)), 1.0 - alpha);
                centre = newstart + (dir * (length * 0.5f));
            }
            else
                centre = start + (dir * (length * 0.5f));
            XMMATRIX translation = XMMatrixTranslationFromVector(centre);

            //PE: Rotate quad to face camera.
            // GG: from the streak's own centre; from the middle of the whole shot, far downrange, a short streak near the gun
            // seen from behind was turned nearly edge on and drawn as a wedge
            const XMFLOAT3 cameraPos = camera.Eye;
            XMVECTOR toCamera = XMVectorSubtract(XMLoadFloat3(&cameraPos), centre);
            toCamera = XMVector3Normalize(toCamera);

            //PE: Rotation matrix
            XMVECTOR right = XMVector3Cross(dir, toCamera);
            // GG: seen exactly end on there is no side to face; any side, rather than a matrix of NaNs
            if (XMVectorGetX(XMVector3LengthSq(right)) < 0.000001f)
                right = XMVector3Cross(dir, fabsf(XMVectorGetY(dir)) < 0.99f ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0));
            right = XMVector3Normalize(right);
            XMVECTOR up = XMVector3Cross(right, dir);

            XMMATRIX rotation = XMMATRIX(
                right * width,
                dir * length,
                up * width,
                XMVectorSet(0, 0, 0, 1)
            );

            XMMATRIX world = rotation * translation;

            //PE: Final World-View-Projection matrix
            XMMATRIX wvp = world * myViewProj;

            alpha = PELerp(alpha, 1.0f, alpha); //PE: Make it fade faster at the end.

            TracerCB cb;
            cb.g_mWorldViewProj = XMMatrixTranspose(wvp);
            cb.g_TintColor = XMFLOAT4(tracer.color.x, tracer.color.y, tracer.color.z, tracer.color.w * alpha);
            cb.g_GlowIntensity = tracer.glowIntensity;
            cb.g_ScrollSpeed = tracer.scrollSpeed;
            cb.g_Time = gameTime;
            cb.g_ScaleV = tracer.scaleV;

            uint32_t bindSlot = 2;
            // GGMAX 2026-08-08: same shared-CB copy race as the gpup sim (task #122) — one
            // constantBuffer re-copied per tracer between draws with no barriers lets a draw
            // read a LATER tracer's matrix/tint under GPU overlap. Per-call transient CB
            // instead (see GPUParticles_part0.cpp noise-pass comment for the full mechanism).
            device->BindDynamicConstantBuffer(cb, bindSlot, cmd);
            uint32_t tID = tracer.tracerID;
            if (tID >= MAXTRACERS + MAXLUATRACERS) tID = 0;   // GGMAX 3.55: was >, one past the end
            // GGMAX 3.55: an unconditional re-bind used to sit here and CLOBBER the white
            // fallback chosen just above. DX11 tolerated it (its texture was always valid);
            // in DX12 an invalid GPUResource binds a NULL descriptor, the sample returns 0,
            // and an additive blend of zero writes nothing - a silently invisible tracer.
            if (tracerTexture[tID].IsValid())
                device->BindResource(&tracerTexture[tID], 0, cmd);
            else
                device->BindResource(wiTextureHelper::getWhite() , 0, cmd);
            device->BindSampler(&samplerTrilinearWrap, 0, cmd);

            const GPUBuffer* vbs[] = { &quadVB };
            const UINT strides[] = { sizeof(XMFLOAT3) + sizeof(XMFLOAT2) };
            const UINT offsets[] = { 0 };
            device->BindVertexBuffers(vbs, 0, 1, strides, 0, cmd);
            device->BindIndexBuffer(&quadIB, IndexBufferFormat::UINT16, 0, cmd);
            device->DrawIndexed(6, 0, 0, cmd);
        }

        wiRenderer::BindCommonResources(cmd);

        device->EventEnd(cmd);

    }

    void CreateQuad()
    {
        GPUBufferDesc bd = {};
        SubresourceData data = {};
        data.data_ptr = vertices;
        bd.usage = Usage::DEFAULT;
        bd.size = sizeof(vertices);
        bd.bind_flags = BindFlag::VERTEX_BUFFER;
        wi::graphics::GetDevice()->CreateBuffer(&bd, data.data_ptr, &quadVB);

        data.data_ptr = indices;
        bd.size = sizeof(indices);
        bd.bind_flags = BindFlag::INDEX_BUFFER;
        wi::graphics::GetDevice()->CreateBuffer(&bd, data.data_ptr, &quadIB);
    }

    void CreatePipelineState()
    {
        PipelineStateDesc desc;
        wiRenderer::LoadShader(ShaderStage::VS, shaderTracerVS, "BulletTracerVS.cso");
        wiRenderer::LoadShader(ShaderStage::PS, shaderTracerPS, "BulletTracerPS.cso");
        if (!shaderTracerVS.IsValid() || !shaderTracerPS.IsValid())
        {
            wi::backlog::post("Tracer system disabled: shaders failed to load", wi::backlog::LogLevel::Warning);
            tracerSystemReady = false;
            return;
        }
        desc.ps = &shaderTracerPS;
        desc.vs = &shaderTracerVS;
        desc.bs = &blendStatesAdditive;
        desc.rs = &rasterizerState;
        desc.dss = &depthStencilState;

        tracerInputLayout.elements = {
            { "POSITION", 0, Format::R32G32B32_FLOAT, 0, 0, InputClassification::PER_VERTEX_DATA },
            { "TEXCOORD", 0, Format::R32G32_FLOAT, 0, 12, InputClassification::PER_VERTEX_DATA }
        };
        desc.il = &tracerInputLayout;

        if (wi::graphics::GetDevice()->CreatePipelineState(&desc, &tracerPSO))
        {
            tracerSystemReady = true;
        }
        else
        {
            wi::backlog::post("Tracer system disabled: pipeline state creation failed", wi::backlog::LogLevel::Warning);
            tracerSystemReady = false;
        }
    }

}
