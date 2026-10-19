//
// GPUParticles system from AGK
//

#include <string>
#include <algorithm>
#include <functional>
#include "GPUParticles.h"
#include "Utility/stb_image.h"
#include "CFileC.h"
#include "Particles/Models/gpup_part_1024.h"
#include "Particles/Models/gpup_part_4096.h"
#include "wiRenderer.h"
#include "WickedEngine.h"

// redefines MAX_PATH to 1050
#include "preprocessor-moreflags.h"

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

using namespace wiGraphics;
using namespace wiScene;

// GGMAX 2026-08-08: engine descriptor-ring forensics bridge (white-out hunt) —
// implemented in wiGraphicsDevice_DX12.cpp, appended to every GPUP_DUMP.
namespace wi::graphics { void GG_GetDescriptorRingStats(char* buf, int bufsize); }

// Load a GPU Particle shader from Particles/Shaders/ with WickedEngine shaders as include path
static bool LoadGPUPShader(ShaderStage stage, Shader& shader, const std::string& filename)
{
	std::string customDir = wi::helper::GetDirectoryFromPath(std::string(__FILE__)) + "Particles/Shaders/";
	wi::helper::MakePathAbsolute(customDir);

	std::string engineShaderDir = wiRenderer::GetShaderSourcePath();
	wi::helper::MakePathAbsolute(engineShaderDir);

	std::string shaderbinaryfilename = wiRenderer::GetShaderPath() + filename;

	// Register for hot-reload tracking and check if cached .cso is up-to-date
	wi::shadercompiler::RegisterShader(shaderbinaryfilename);

	if (wi::shadercompiler::IsShaderOutdated(shaderbinaryfilename))
	{
		wi::shadercompiler::CompilerInput input;
		input.format = wiGraphics::GetDevice()->GetShaderFormat();
		input.stage = stage;
		input.include_directories.push_back(engineShaderDir);
		input.include_directories.push_back(customDir);
		input.shadersourcefilename = wi::helper::ReplaceExtension(customDir + filename, "hlsl");

		wi::shadercompiler::CompilerOutput output;
		wi::shadercompiler::Compile(input, output);

		if (output.IsValid())
		{
			wi::shadercompiler::SaveShaderAndMetadata(shaderbinaryfilename, output);
			wi::backlog::post("gpup shader compiled: " + shaderbinaryfilename);
			return wiGraphics::GetDevice()->CreateShader(stage, output.shaderdata, output.shadersize, &shader);
		}
		else
		{
			wi::backlog::post("gpup shader compile FAILED: " + filename + "\n" + output.error_message, wi::backlog::LogLevel::Error);
			return false;
		}
	}

	// Cache is up-to-date, load from .cso file
	wi::vector<uint8_t> buffer;
	if (wi::helper::FileRead(shaderbinaryfilename, buffer))
	{
		return wiGraphics::GetDevice()->CreateShader(stage, buffer.data(), buffer.size(), &shader);
	}

	return false;
}

// ★ GGMAX 3.35j: diagnostic files go beside the EXE, not into the CWD. Declared HERE, BEFORE
// `namespace GPUParticles` opens - inside it this would declare GPUParticles::GGDiagFopen and
// fail to link. Defined in Guru-WickedMAX/master_part1.cpp.
extern FILE* GGDiagFopen(const char* name, const char* mode);

extern bool g_bGamePaused;

namespace GPUParticles
{
// file functions, only support one file open at a time
FILE* gpup_file = 0;
int GetFileExists( const char* filename ) 
{ 
	char fullPath[ MAX_PATH ];
	strcpy_s( fullPath, MAX_PATH, filename );
	GG_GetRealPath( fullPath, 0 );
	FILE *test = 0;
	int err = fopen_s( &test, fullPath, "rb" );
	if ( err == 0 )
	{
		fclose( test );
		return 1;
	}

	return 0; 
}

int OpenToRead( int fileID, const char* filename ) 
{ 
	char fullPath[ MAX_PATH ];
	strcpy_s( fullPath, MAX_PATH, filename );
	GG_GetRealPath( fullPath, 0 );
	int err = fopen_s( &gpup_file, fullPath, "rb" ); //PE: reading from docwrite failed here. (filename)
	if ( err == 0 ) return 1;
	else return 0;
}

void AGKDumpString( int fileID ) 
{  
	if ( !gpup_file ) return;

	char c;
	do
	{
		c = fgetc( gpup_file );
	} while( c != 0 && !feof(gpup_file) );
}

float AGKReadFloat( int fileID ) 
{ 
	if ( !gpup_file ) return 0;
	
	float f;
	fread( &f, 4, 1, gpup_file );
	return f;
}

int ReadInteger( int fileID ) 
{ 
	if ( !gpup_file ) return 0;

	int i = 0;
	fread( &i, 4, 1, gpup_file );
	return i;
}

void AGKCloseFile( int fileID ) 
{ 
	if ( !gpup_file ) return;
	fclose( gpup_file );
	gpup_file = 0;
}

int64_t i64StartTime = 0;
int64_t i64TimeFreq = 1;
void AGKTimerInit()
{
	QueryPerformanceCounter ( (LARGE_INTEGER*) &i64StartTime );
	QueryPerformanceFrequency ( (LARGE_INTEGER*) &i64TimeFreq );
	if ( i64TimeFreq == 0 ) i64TimeFreq = 1;
}

// Timer in seconds
float AGKTimer()
{
	int64_t i64CurrentTime;
	QueryPerformanceCounter ( (LARGE_INTEGER*) &i64CurrentTime );
	i64CurrentTime -= i64StartTime;
	return (float) (i64CurrentTime / (double) i64TimeFreq);
}

uint32_t iRandSeed = 0;
void RandomInit()
{
	int64_t i64CurrentTime;
	QueryPerformanceCounter ( (LARGE_INTEGER*) &i64CurrentTime );
	iRandSeed = (uint32_t) i64CurrentTime;
}

// produces a number between 0 and 65535
int Random() 
{ 
	// Uses integer overflow to generate pseudo random numbers.
	iRandSeed = (214013*iRandSeed + 2531011);
	// only use the top 16 bits as the lower 16 bits produce very short repeat cycles.
	return (iRandSeed >> 16) & 0xffff;
	return 0; 
}

int iRandMTIndex = 0;
uint32_t iRandMTArray[ 624 ];
void Random2Init()
{
	int64_t i64CurrentTime;
	QueryPerformanceCounter ( (LARGE_INTEGER*) &i64CurrentTime );

	iRandMTIndex = 0;
	iRandMTArray[0] = (uint32_t) i64CurrentTime;
	for ( int i = 1; i < 623; i++ )
	{
		iRandMTArray[i] = (1812433253 * (iRandMTArray[i-1] ^ (iRandMTArray[i-1] >> 30)) + i);
	}
}

// produces a number between -2,147,483,648 and 2,147,483,647
int Random2() 
{ 
	if ( iRandMTIndex == 0 ) 
	{
		for ( int i = 0; i < 624; i++ ) 
		{
			uint32_t y = (iRandMTArray[i] & 0x80000000) + (iRandMTArray[(i+1) % 624] & 0x7fffffff);
			iRandMTArray[i] = iRandMTArray[(i + 397) % 624] ^ (y>>1);
			if ( (y % 2) != 0 ) iRandMTArray[i] = iRandMTArray[i] ^ 0x9908b0df;
		}
	}

	uint32_t y = iRandMTArray[iRandMTIndex];
	y = y ^ (y >> 11);
	y = y ^ ((y << 7) & 0x9d2c5680);
	y = y ^ ((y << 15) & 0xefc60000);
	y = y ^ (y >> 18);

	iRandMTIndex = (iRandMTIndex + 1) % 624;
	return (int) y;
} 

//PE: Changed from 32 to 64.
#define gpup_maxeffects 64

#define gpup_particles_1k    32
#define gpup_particles_4k    64
#define gpup_particles_16k   128
#define gpup_particles_64k   256
#define gpup_particles_256k  512
#define gpup_particles_1M    1024

#define gpup_particles_opaque    1
#define gpup_particles_clipped   2
#define gpup_particles_alpha     3
#define gpup_particles_additive  4

#define gpup_active    1
#define gpup_inactive  0

// MainVS constants
struct sMainVSConstantData
{
	float4x4 World;
	float4x4 View;
	float4x4 Proj;
	float4x4 ViewProj;
	float4 globalpos[8];
	float4 tilex;
	float4 particles;
	float3 pgrow;
	float padding1;
	float3 ppos;
	float padding2;
	float3 area;
	float padding3;
	float3 rotat;
	float padding4;
	float3 rota;
	float padding5;
	float3 image_count;
	float padding6;
	float3 globalsize;
	float padding7;
	float3 globalrot;
	float padding8;
	float3 CameraPos;
	float padding9;
	float2 pcolor;
	float padding10;
	float padding11;
};

struct sMainPSConstantData
{
	float4 tile;
	float3 clr;
	float moder;
	float3 image_count;
	float agk_time;
	float opacity;
	float filler;  //PE: Must be 16 floats so, reserved for later.
	float filler2;
	float filler3;
};

struct sPosConstantData
{								// offset
	float4 gravity;				// 0
	float4 emittersize;			// 16
	float4 emitterrotation;		// 32
	float3 emitterline;			// 48
	float emittertype;			
	float3 emittersway;			// 64
	float agk_time;
	float3 localemitter;		// 80
	float warp;
	float3 pos1;				// 96
	float rnd;
	float3 pos2;				// 112
	float padding1;
	float3 emitterlinecount;	// 128
	float padding2;
	float3 moveit;				// 144
	float padding3;
	float3 particles;			// 160
	float padding4;
	float3 area;				// 176
	float padding5;
	float2 spawnpos;			// 192
	float2 lifespan;			// 200
};

struct sSpeedConstantData
{								// offset
	float4 area;				// 0
	float4 gravity;				// 16
	float4 speedvar;			// 32
	float4 field;				// 48
	float4 autorot;				// 64
	float4 reffloor;			// 80
	float4 refsphere;			// 96
	float3 particles;			// 112
	float rnd;					// 124
	float2 spawnpos;			// 128
	float warp;					// 136
	float rnd2;					// 140
};

struct sNoiseConstantData
{
	float3 off;
	float paddingsobuffermultipleofsixteen; // :)
};

sNoiseConstantData noiseConstantData;

class t_gpup_settings
{
public:
	int   simulOn = 0;
	float gtimer = 0;
	float time = 0;
	int   spawnCount = 0;
	int   pauser = 0;
	float tmr = 0;
	float sn = 0;
	float rotsn = 0;
		
	int   emitterCount = 0;
	int   lastEmitter = 0;
	int   activeEffects1 = 0;
	int   activeEffects2 = 0;
	int   activeEffects = 0;
			
	int   vrx = 0;
	int   vry = 0;
	int   split = 0;

	t_gpup_settings() {}
	~t_gpup_settings() {}
};

t_gpup_settings gpup_settings;

class t_gpup_emitter
{
public:
	std::string name;
	
	float x = 0;
	float y = 0;
	float z = 0;
	float scale = 0;
	int   particles = 0;
	float fsize = 0;
	int   frez = 0;
	int   ani_x = 0;
	int   ani_y = 0;
	int   ani_s = 0;
	
	int   effectVisible = 1;
	
	int   image_count = 0;	
	int   layer_count = 0;
	
	float area = 0;
	float varea = 0;
	float narea = 0;
	
	float r = 0;
	float g = 0;
	float b = 0;
	float opacity = 1.0;
	int   blendmode = 0;
	int   col = 0;
	int   facing = 0;
	
	float field_adhesion = 0;
	float field_x = 0;
	float field_y = 0;
	float field_z = 0;
	
	float noise_adhesion = 0;
	float offset_speed = 0;
	float auto_rot_x = 0;
	float auto_rot_y = 0;
	float auto_rot_z = 0;
	float noise_bias = 0;
	
	float gravity_x = 0;
	float gravity_y = 0;
	float gravity_z = 0;
	
	float lifespan = 0;
	float lifespan_variance = 0;
	float maxed = 0;
	
	int   emitter_type = 0;
	float emitter_local_x = 0;
	float emitter_local_y = 0;
	float emitter_local_z = 0;
	float emitter_size_x = 0;
	float emitter_size_y = 0;
	float emitter_size_z = 0;
	float emitter_radius_1 = 0;
	float emitter_radius_2 = 0;
	float emitter_height = 0;
	float emitter_x1 = 0;
	float emitter_y1 = 0;
	float emitter_z1 = 0;
	float emitter_x2 = 0;
	float emitter_y2 = 0;
	float emitter_z2 = 0;
	float emitter_rotation_x = 0;
	float emitter_rotation_y = 0;
	float emitter_rotation_z = 0;
	float emitter_speed_x = 0;
	float emitter_speed_y = 0;
	float emitter_speed_z = 0;
	float emitter_speed_v = 0;

	float emitter_speedadjustment_x = 0.5; // aded to rotate the speed vector (so can change direction of emission)
	float emitter_speedadjustment_y = 0.5; //PE: 0.5 = neutral.
	float emitter_speedadjustment_z = 0.5;

	int   emitter_wire = 0;
	int   emitter_corners = 0;
	float emitter_auto_rotation_x = 0;
	float emitter_auto_rotation_y = 0;
	float emitter_auto_rotation_z = 0;
	int   emitter_auto_movement = 0;
	float emitter_line_sway = 0;
	float emitter_line_sway_size = 0;
	float emitter_line_sway_speed = 0;
	float emitter_lineswirl = 0;
	float emitter_linebeams = 0;
	float emitter_linebeamoffset = 0;
	
	int   emitter_slow_mode = 0;
	float emitter_amount = 0;
	
	int   emitter_burst_mode = 0;
	int   emitter_burst_auto = 0;
	float emitter_burst_delay = 0;
	int   emitter_burst_frames = 0;
	int   emitter_burst_fire = 0;
	float emitter_burst_delaycount = 0;
	
	int   emitter_moving = 0;
	float emitter_moving_speed = 0;
	
	float image_distortion = 0;
	float image_distortion_speed = 0;
	
	float size1 = 0;
	float size2 = 0;
	float rotation = 0;
	float rotation_variance = 0;
	float sizer = 0;
		
	int	  currImage = 0;
	Texture texPos[2]; // point sample
	Texture texSpeed[2]; // point sample
	Texture texNoise; // linear sample
	Texture t_field; // linear sample

	RenderPass renderPassSpeed[ 2 ];
	RenderPass renderPassPos[ 2 ];
	RenderPass renderPassNoise;

	sMainVSConstantData mainVSConstantData = {};
	sMainPSConstantData mainPSConstantData = {};
	sPosConstantData posConstantData = {};
	sSpeedConstantData speedConstantData = {};
	
	float noiseOff1 = 0;
	float noiseOff2 = 0;
	int   emiton = 0;
	
	float globalx[8] = {0};
	float globaly[8] = {0};
	float globalz[8] = {0};
	
	float newX = 0;
	float newY = 0;
	float newZ = 0;
	int   pivotMoved = 0;
	
	float globalSize = 0;
	
	float globalRotX = 0;
	float globalRotY = 0;
	float globalRotZ = 0;
	
	int   testpos = 0;
	float spawnint = 0;
	int   effectLoaded = 0;
	
	float noiseZaehler = 0;
	int   noiseDir = 0;
	
	Texture image1;
	Texture imagex;
	Texture gradient_1;
	
	float bounciness = 0;
	float floorActive = 0;
	float sphereActive = 0;
	float sphereRadius = 0;
	float floorHeight = 0;
	float sphereX = 0;
	float sphereY = 0;
	float sphereZ = 0;
	
	int   spawnCount = 0;
	int   emitterActive = 0;
	int   subEmitters = 0;
	int   subEmitterUsed[8] = {0};
	float particleSize = 0;
	float activeTimer = 0;
	int   smooth_blending = 0;

	float emitter_animation_speed = 1.0f;

	float currentdistancefromcamera = 0.0f;

	t_gpup_emitter() {}
	~t_gpup_emitter() {}
};

t_gpup_emitter gpup_emitter[ gpup_maxeffects ];

// GGMAX 2026-08-07 parity instruments (task #121) — read by the harness GPUP_DUMP/GPUP_SHOW/
// SET_GPUPARM commands. The counters prove the sim cadence live: after the dt cap fix,
// gg_gpup_max_time must never exceed 0.0333/0.015 = 2.22 (it hit up to 33.3 before).
int gg_gpup_force_arm = 0;          // 0 = stock branch logic; 1 = force whole-batch arm; 2 = force split arm
int gg_gpup_draw_enable = 1;        // GPUP_SHOW 0|1 — attribution lever: skip all gpup drawing when 0
uint64_t gg_gpup_doit_calls = 0;    // per-emitter sim steps taken
double   gg_gpup_time_sum = 0.0;    // sum of the per-step warp multipliers (gpup_settings.time)
float    gg_gpup_max_time = 0.0f;   // worst single-step warp seen since init
uint64_t gg_gpup_arm_whole = 0;     // update ticks taken by the whole-batch arm
uint64_t gg_gpup_arm_split = 0;     // update ticks taken by the split arm


// rendering variables

struct VertexQuad
{
    float x,y;
};

VertexQuad g_VerticesQuad[6] = 
{
    { -1.0f,  1.0f, },
    { -1.0f, -1.0f, }, 
    {  1.0f,  1.0f, }, 
    {  1.0f,  1.0f, }, 
    { -1.0f, -1.0f, }, 
    {  1.0f, -1.0f, }  
};

// GPUP variables
GPUBuffer mainIndexBufferObj0;
GPUBuffer mainIndexBufferObj1;
int mainIndexCountObj0 = 0;
int mainIndexCountObj1 = 0;
GPUBuffer mainVertexBufferObj0;
GPUBuffer mainVertexBufferObj1;

PipelineState psoAlpha;
PipelineState psoAdd;
PipelineState psoOpaque;

Shader shaderMainVS;
Shader shaderMainPS;

GPUBuffer mainVSConstants;
GPUBuffer mainPSConstants;

// quad variables
PipelineState psoQuadDefault;
Shader shaderQuadVS;
Shader shaderQuadDefaultPS;
GPUBuffer quadVertexBuffer;

Shader shaderNoisePS;
Shader shaderSpeedPS;
Shader shaderPosPS;

GPUBuffer speedConstants;
GPUBuffer posConstants;
GPUBuffer noiseConstants;

PipelineState psoNoise;
PipelineState psoPos;
PipelineState psoSpeed;

Sampler samplerPoint;
Sampler samplerLinear;
Sampler samplerLinearWrap;

Texture texNoiseOrig;
Texture texDist2;

int gpu_particles_initialised = 0;


void GPUP_LoadTexture( const char* filename, Texture* tex )
{
	GraphicsDevice* device = wiGraphics::GetDevice();

	int width, height, channels;
	char filePath[ MAX_PATH ];
	strcpy_s( filePath, MAX_PATH, filename );
	GG_GetRealPath( filePath, 0 );
	uint8_t* imageData = stbi_load( filePath, &width, &height, &channels, 4 );
	if ( !imageData )
	{
		wi::backlog::post(std::string("GPUP_LoadTexture: failed to load ") + filename, wi::backlog::LogLevel::Error);
		return;
	}

	SubresourceData data = {};
	data.data_ptr = imageData;
	data.row_pitch = width * 4;

	TextureDesc texDesc = {};
	texDesc.bind_flags = BindFlag::SHADER_RESOURCE;
	texDesc.sample_count = 1;
	texDesc.mip_levels = 1;
	texDesc.array_size = 1;
	texDesc.format = Format::R8G8B8A8_UNORM;
	texDesc.usage = Usage::DEFAULT;
	texDesc.width = width;
	texDesc.height = height;

	device->CreateTexture( &texDesc, &data, tex );
	device->SetName( tex, "imageTex" );

	stbi_image_free( imageData );
}

// GGMAX 2026-08-07 (task #121): DX11 zero-filled these sim render textures at creation;
// the Feb port dropped the initial data, so texPos/texSpeed/texNoise started as GPU garbage.
// With the sim renderpasses on LoadOp::DONTCARE, a freshly loaded emitter could draw one or
// more frames of random packed positions/ages before the first full sim pass overwrote them
// (contributor to the "particles reset/glitch" report). Restored to DX11 behaviour.
void GPUP_CreateRenderTexture( int width, int height, Texture* tex )
{
	GraphicsDevice* device = wiGraphics::GetDevice();

	TextureDesc texDesc = {};
	texDesc.bind_flags = BindFlag::RENDER_TARGET | BindFlag::SHADER_RESOURCE;
	texDesc.clear.color[0] = 0.0f;
	texDesc.clear.color[1] = 0.0f;
	texDesc.clear.color[2] = 0.0f;
	texDesc.clear.color[3] = 0.0f;
	texDesc.sample_count = 1;
	texDesc.mip_levels = 1;
	texDesc.array_size = 1;
	texDesc.format = Format::R8G8B8A8_UNORM;
	texDesc.usage = Usage::DEFAULT;
	texDesc.width = width;
	texDesc.height = height;

	// zero initial contents (DX11 parity — see comment above)
	wi::vector<uint32_t> zeroData;
	zeroData.resize((size_t)width * (size_t)height, 0u);
	SubresourceData initData = {};
	initData.data_ptr = zeroData.data();
	initData.row_pitch = width * 4;
	device->CreateTexture( &texDesc, &initData, tex );
	device->SetName( tex, "renderTex" );
}

// GGMAX 3.65 VRAM LEAK: this was an EMPTY STUB - it fetched the device and returned, so every
// caller in gpup_deleteEffect() freed nothing. gpup_deleteAllEffects() runs on every level load
// (gridedit_clear_map), so the whole teardown chain was wired up correctly and landed on a
// no-op: an .arx emitter's textures and sim render targets stayed resident for the life of the
// process. Measured with the VRAM census: after one visit to RPG Template, Operation Amazon held
// 53 "imageTex"/"renderTex" records where a cold load has 3 (+27.9 MB, and it grows with every
// distinct level visited).
//
// Assigning a default-constructed Texture drops this handle's reference to the internal state;
// the resource is released once the device's deferred-destroy queue drains, which the level-load
// path already forces immediately afterwards via WickedCall_ReloadQuiesceGPU().
void GPUP_DeleteTexture( Texture* tex )
{
	if ( tex == nullptr ) return;
	*tex = Texture();
}

void GPUParticlesDrawQuad( RenderPass* renderPass, CommandList cmd )
{
	GraphicsDevice* device = wiGraphics::GetDevice();

	device->RenderPassBegin( renderPass, cmd );

	Viewport vp;
	vp.width = (float) renderPass->GetDesc().attachments[0].texture.GetDesc().width;
	vp.height = (float) renderPass->GetDesc().attachments[0].texture.GetDesc().height;
	device->BindViewports( 1, &vp, cmd );
			
	const GPUBuffer* vbs[] = { &quadVertexBuffer };
	uint32_t stride = sizeof( VertexQuad );
	device->BindVertexBuffers( vbs, 0, 1, &stride, 0, cmd );
	
	// bind samplers
	device->BindSampler(&samplerLinear, 0, cmd );
	device->BindSampler(&samplerPoint, 1, cmd );
	device->BindSampler(&samplerLinearWrap, 2, cmd );

	device->Draw( 6, 0, cmd );

	device->RenderPassEnd( cmd );
}

// GGMAX 2026-08-07 gpup restore: the DX11 fork indexed these by the integer CommandList to
// support the per-object transparent interleave (its wiRenderer.cpp:3546 called
// gpup_draw_bydistance inside the batch loop, potentially on several command lists). The DX12
// port stubbed everything "CommandList is no longer an integer index". DX12 draws gpup as ONE
// whole sorted batch per frame from a single command list (customDraw_Transparent hook in
// master_part1.cpp), so the per-cmd dimension is simply gone — plain per-frame statics.
uint32_t g_emitterSorting[gpup_maxeffects];
uint32_t g_emitterCurrentIndex;

extern "C" void gpup_draw_init(const wiScene::CameraComponent & camera, wiGraphics::CommandList cmd)
{
	if ( !gpu_particles_initialised ) return;
	// calculates all needed particles and distances from camera
	for (size_t i = 0; i < gpup_maxeffects; ++i)
	{
		g_emitterSorting[i] = 0;
		g_emitterSorting[i] |= (uint32_t)i & 0x0000FFFF;
		if (gpup_emitter[i].effectLoaded == 0 || gpup_emitter[i].effectVisible == 0) continue;
		XMFLOAT3 emitercenter = XMFLOAT3(gpup_emitter[i].globalx[0], gpup_emitter[i].globaly[0], gpup_emitter[i].globalz[0]);
		float distance = wiMath::DistanceEstimated(XMFLOAT3(emitercenter.x, camera.Eye.y, emitercenter.z), camera.Eye);
		gpup_emitter[i].currentdistancefromcamera = distance;
		// DX11-verbatim key: distance decihash in the HIGH 16 bits (wraps past 6553.5 units,
		// exactly as the DX11 product did), emitter index in the LOW 16.
		g_emitterSorting[i] |= ((uint32_t)(distance * 10) & 0x0000FFFF) << 16;
	}

	// now sort so distant particles are first in list
	std::sort(std::begin(g_emitterSorting), std::end(g_emitterSorting), std::greater<uint32_t>());

	// we start here, being the particle furthest away
	g_emitterCurrentIndex = 0;
}

extern "C" void gpup_draw_bydistance(const wiScene::CameraComponent & camera, wiGraphics::CommandList cmd, float fDistanceFromCamera)
{
	// GGMAX 2026-08-07 gpup restore: STILL UNCALLED in DX12 — this was the DX11 engine fork's
	// per-object transparent interleave entry (its wiRenderer.cpp:3546 called it inside the
	// batch loop so particles sorted BETWEEN transparent meshes: window, particle, window).
	// The DX12 clone has no such hook; the DX12 path is the whole sorted batch in gpup_draw()
	// from the customDraw_Transparent callback (master_part1.cpp). If per-object interleave
	// parity is ever wanted, this body needs the single-dim sort arrays above plus an engine
	// callback inside RenderMeshes' transparent loop — an engine delta, deliberately not taken.
	return;

	// is called just before a transparent object is rendered at the specified distance
	// allowing us to insert the particle rendering as needed (i.e. window, particle, window, window, particle, window)
	int iThisLoopStart = 0;
	for (size_t i = iThisLoopStart; i < gpup_maxeffects; ++i)
	{
		// TODO: CommandList is no longer an integer index
		//size_t e = g_emitterSorting[cmd][i] & 0x0000FFFF;
		size_t e = i;

		// clever bit - if this particle distance is further than the current distance from the camera,
		// we need to render it now as the transparent object from Wicked will be rendered next and particle
		// needs to be behind it!
		if (e >= 0 && e < gpup_maxeffects && gpup_emitter[e].currentdistancefromcamera > fDistanceFromCamera)
		{
			// we carry on and render this particle at least
			// and will loop around to see if the next particle is 
			// is further than the current distance
		}
		else
		{
			// we stop right now, this particle is closer than the current distance
			// and we cannot render it yet			
			return;
		}

		// next time we are ready for next particle after this one
		// TODO: CommandList is no longer an integer index
		//g_emitterCurrentIndex[cmd] = i + 1;

		if (e < 0 || e >= gpup_maxeffects || gpup_emitter[e].effectLoaded == 0 || gpup_emitter[e].effectVisible == 0) continue;

		GraphicsDevice* device = wiGraphics::GetDevice();
		device->EventBegin("GPUParticles Draw", cmd);

		gpup_emitter[e].mainVSConstantData.Proj = camera.Projection;
		gpup_emitter[e].mainVSConstantData.View = camera.View;

		XMMATRIX proj((float*)&camera.Projection);
		XMMATRIX viewProj((float*)&camera.View);
		viewProj *= proj;
		memcpy(gpup_emitter[e].mainVSConstantData.ViewProj.m, viewProj.r, sizeof(float) * 16);

		gpup_emitter[e].mainVSConstantData.CameraPos.x = camera.Eye.x;
		gpup_emitter[e].mainVSConstantData.CameraPos.y = camera.Eye.y;
		gpup_emitter[e].mainVSConstantData.CameraPos.z = camera.Eye.z;

		float scale = 1.0f;
		gpup_emitter[e].mainVSConstantData.World.m[0][0] = scale;
		gpup_emitter[e].mainVSConstantData.World.m[0][1] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[0][2] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[0][3] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[1][0] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[1][1] = scale;
		gpup_emitter[e].mainVSConstantData.World.m[1][2] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[1][3] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[2][0] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[2][1] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[2][2] = scale;
		gpup_emitter[e].mainVSConstantData.World.m[2][3] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[3][3] = 1;

		//PE: Blend state is not always set when changing from psoAlpha to psoAdd, add different blendfactor's to force change (not visible).
		//PE: TODO - psoOpaque -> psoAdd has another change problem, must be desc.dss = &depthDesc;
		//PE: TODO - As we dont use sprites currently, leave this for after EA. ( REMOVED_EARLYACCESS )
		switch (gpup_emitter[e].blendmode)
		{
			case 0: // opaque
			case 1: // opaque
			case 2: // opaque
			{
				if (gpup_emitter[e].mainPSConstantData.opacity != 1.0f)
				{
					device->BindPipelineState(&psoAlpha, cmd); //If using opacity switch to alpha blend.
					device->BindBlendFactor(1.0, 1.0, 1.0, 1.0, cmd);
				}
				else
				{
					device->BindPipelineState(&psoOpaque, cmd); //Sprites
					device->BindBlendFactor(1.0, 1.0, 0.998, 1.0, cmd);
				}

			} break;

			case 3: // alpha
			{
				device->BindPipelineState(&psoAlpha, cmd);
				device->BindBlendFactor(1.0, 1.0, 1.0, 1.0, cmd);
			} break;

			case 4: // additive
			{
				device->BindPipelineState(&psoAdd, cmd); //Particles
				device->BindBlendFactor(1.0, 1.0, 0.999, 1.0, cmd);
			} break;

			default:
			{
				device->BindPipelineState(&psoAdd, cmd);
				device->BindBlendFactor(1.0, 1.0, 0.999, 1.0, cmd);
			} break;
		}

		gpup_emitter[e].mainPSConstantData.agk_time = AGKTimer();

		// in-render-pass safe form (see gpup_draw); UpdateBuffer here would remove the device
		device->BindDynamicConstantBuffer(gpup_emitter[e].mainPSConstantData, 1, cmd);

		int numIndices = 0;
		const GPUBuffer* vbs[1];
		if (gpup_emitter[e].particles == 32)
		{
			vbs[0] = &mainVertexBufferObj1;
			device->BindIndexBuffer(&mainIndexBufferObj1, IndexBufferFormat::UINT16, 0, cmd);
			numIndices = mainIndexCountObj1;
		}
		else
		{
			vbs[0] = &mainVertexBufferObj0;
			device->BindIndexBuffer(&mainIndexBufferObj0, IndexBufferFormat::UINT16, 0, cmd);
			numIndices = mainIndexCountObj0;
		}

		uint32_t stride = sizeof(GPUP_1024_Vertex); // 3 position + 2 uv
		device->BindVertexBuffers(vbs, 0, 1, &stride, 0, cmd);

		// bind textures
		device->BindResource(&gpup_emitter[e].texPos[gpup_emitter[e].currImage], 0, cmd);
		device->BindResource(&gpup_emitter[e].gradient_1, 2, cmd);
		device->BindResource(&gpup_emitter[e].texSpeed[gpup_emitter[e].currImage], 5, cmd);
		device->BindResource(&gpup_emitter[e].imagex, 1, cmd);
		device->BindResource(&gpup_emitter[e].image1, 3, cmd);
		device->BindResource(&texDist2, 4, cmd);

		// bind samplers
		device->BindSampler(&samplerLinear, 0, cmd);
		device->BindSampler(&samplerPoint, 1, cmd);
		device->BindSampler(&samplerLinearWrap, 2, cmd);
		device->BindSampler(&samplerPoint, 1, cmd);

		int ii = gpup_emitter[e].particles / 64;
		if (gpup_emitter[e].particles == 32) ii = 1;

		// TODO: CommandList is no longer an integer index
		//if (g_emitterSortingDrawCount[cmd][i] == 0)
		if (true)
		{
			for (int i = 0; i < ii; i++)
			{
				for (int j = 0; j < ii; j++)
				{
					gpup_emitter[e].mainVSConstantData.World.m[3][0] = 64.0f * i;
					gpup_emitter[e].mainVSConstantData.World.m[3][1] = 0.0f;
					gpup_emitter[e].mainVSConstantData.World.m[3][2] = 64.0f * j;

					device->BindDynamicConstantBuffer(gpup_emitter[e].mainVSConstantData, 0, cmd);

					device->DrawIndexed(numIndices, 0, 0, cmd);
				}
			}
		}
		device->EventEnd(cmd);

		// increment draw count for this emitter
		// TODO: CommandList is no longer an integer index
		//g_emitterSortingDrawCount[cmd][i]++;
	}
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine RenderPath3D::RenderTransparents()
// GGMAX 2026-08-08 (task #122 round 5): -1 = draw all; >=0 = draw ONLY that emitter.
// The 30fps window captures show a plume vanishing within ONE frame while the pool is
// proven static — soloing names WHOSE draw flips.
int gg_gpup_solo = -1;
void gpup_debug_solo( int enr ) { gg_gpup_solo = enr; }

// GGMAX 3.110: draw ONE effect with any camera - the Particle Library live preview draws the
// hovered effect into its own render target with the preview camera (master_part2.cpp).
// The effect is kept INVISIBLE to the main view (which would otherwise draw it off-screen, 39000
// units up, every frame), so the visible flag is lifted for this one call. Simulation ignores the
// flag (gpup_update), so a hidden effect keeps animating.
// ⚠ MAIN THREAD ONLY: it goes through gpup_draw, which re-sorts into the shared g_emitterSorting
// and writes the effect's CPU-side constants - never call it from a render job.
void gpup_draw_single( int ID, const CameraComponent& camera, CommandList cmd )
{
	if ( ID < 0 || ID >= (int)gpup_maxeffects || gpup_emitter[ID].effectLoaded == 0 ) return;
	const int oldSolo = gg_gpup_solo;
	const int oldVisible = gpup_emitter[ID].effectVisible;
	gg_gpup_solo = ID;
	gpup_emitter[ID].effectVisible = 1;
	gpup_draw( camera, cmd );
	gpup_emitter[ID].effectVisible = oldVisible;
	gg_gpup_solo = oldSolo;
}

// GGMAX 2026-08-08 (task #122 round 7): force full descriptor re-dirty per gpup draw —
// see the decoy-bind block in gpup_draw.
int gg_gpup_rebind = 0;
void gpup_debug_rebind( int on ) { gg_gpup_rebind = on; }

// GGMAX 2026-08-08 (task #122 round 5d): per-FRAME draw flight recorder. Solo attribution
// convicted emitter 2's draw alone; every 1Hz instrument shows frozen inputs — so record
// the draw's actual inputs EVERY RENDERED FRAME (gpup_drawlog.csv in the game CWD =
// Max/Files). Aligned against a concurrent 30fps window capture, whichever field steps at
// a ping instant is the driver.
int gg_gpup_drawlog = 0;
void gpup_debug_drawlog( int on ) { gg_gpup_drawlog = on; }
static FILE* gg_drawlog_file = nullptr;
static void GG_DrawLogLine( size_t e )
{
	if ( !gg_gpup_drawlog )
	{
		if ( gg_drawlog_file ) { fclose( gg_drawlog_file ); gg_drawlog_file = nullptr; }
		return;
	}
	if ( !gg_drawlog_file )
		gg_drawlog_file = GGDiagFopen("gpup_drawlog.csv", "w");
	if ( !gg_drawlog_file ) return;
	const t_gpup_emitter& em = gpup_emitter[e];
	fprintf( gg_drawlog_file, "%.4f,%u,%d,%.3f,%.4f,%.5f,%.1f,%.1f,%.4f,%.4f,%.5f,%.5f,%.5f,%d,%.4f\n",
		AGKTimer(), (unsigned)wiGraphics::GetDevice()->GetFrameCount(), em.currImage,
		gpup_settings.sn, gpup_settings.time, gpup_settings.gtimer,
		em.posConstantData.spawnpos.x, em.posConstantData.spawnpos.y,
		em.posConstantData.warp, em.mainPSConstantData.agk_time,
		em.mainVSConstantData.rota.x, em.mainVSConstantData.rota.y, em.mainVSConstantData.rota.z,
		em.blendmode, em.mainPSConstantData.opacity );
	fflush( gg_drawlog_file );
}

extern "C" void gpup_draw( const CameraComponent& camera, CommandList cmd )
{
	if ( !gpu_particles_initialised ) return;
	if ( !gg_gpup_draw_enable ) return; // GPUP_SHOW 0 — attribution lever (task #120/#121)
	// cannot do any quad rendering in here as we are already in a render pass by this point
	// only render final emitter objects

	//PE: Quick dist sorting from emitter center to camera.
	gpup_draw_init(camera, cmd);

	for( size_t i = 0; i < gpup_maxeffects; ++i )
	{
		// GGMAX 2026-08-07 gpup restore: back-to-front via the sort gpup_draw_init just built
		// (distance decihash in the high 16 bits, descending — farthest emitter first).
		size_t e = g_emitterSorting[i] & 0x0000FFFF;

		if ( e < 0 || e >= gpup_maxeffects || gpup_emitter[e].effectLoaded == 0 || gpup_emitter[e].effectVisible == 0 ) continue;
		if ( gg_gpup_solo >= 0 && (int)e != gg_gpup_solo ) continue; // GPUP_SOLO lever
		if ( e == 2 ) GG_DrawLogLine( e ); // flight recorder (task #122 round 5d)

		GraphicsDevice* device = wiGraphics::GetDevice();
		device->EventBegin("GPUParticles Draw", cmd);

		gpup_emitter[e].mainVSConstantData.Proj = camera.Projection;
		gpup_emitter[e].mainVSConstantData.View = camera.View;

		XMMATRIX proj( (float*) &camera.Projection );
		XMMATRIX viewProj( (float*) &camera.View );
		viewProj *= proj;
		memcpy( gpup_emitter[e].mainVSConstantData.ViewProj.m, viewProj.r, sizeof(float) * 16 );

		gpup_emitter[e].mainVSConstantData.CameraPos.x = camera.Eye.x;
		gpup_emitter[e].mainVSConstantData.CameraPos.y = camera.Eye.y;
		gpup_emitter[e].mainVSConstantData.CameraPos.z = camera.Eye.z;

		float scale = 1.0f;
		gpup_emitter[e].mainVSConstantData.World.m[0][0] = scale;
		gpup_emitter[e].mainVSConstantData.World.m[0][1] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[0][2] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[0][3] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[1][0] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[1][1] = scale;
		gpup_emitter[e].mainVSConstantData.World.m[1][2] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[1][3] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[2][0] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[2][1] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[2][2] = scale;
		gpup_emitter[e].mainVSConstantData.World.m[2][3] = 0;
		gpup_emitter[e].mainVSConstantData.World.m[3][3] = 1;

		//PE: Blend state is not always set when changing from psoAlpha to psoAdd, add different blendfactor's to force change (not visible).
		//PE: TODO - psoOpaque -> psoAdd has another change problem, must be desc.dss = &depthDesc;
		//PE: TODO - As we dont use sprites currently, leave this for after EA. ( REMOVED_EARLYACCESS )
		switch( gpup_emitter[e].blendmode )
		{
			case 0: // opaque
			case 1: // opaque
			case 2: // opaque
			{
				if (gpup_emitter[e].mainPSConstantData.opacity != 1.0f)
				{
					device->BindPipelineState(&psoAlpha, cmd); //If using opacity switch to alpha blend.
					device->BindBlendFactor(1.0, 1.0, 1.0, 1.0, cmd);
				}
				else
				{
					device->BindPipelineState(&psoOpaque, cmd); //Sprites
					device->BindBlendFactor(1.0, 1.0, 0.998, 1.0, cmd);
				}

			} break;

			case 3: // alpha
			{
				device->BindPipelineState( &psoAlpha, cmd );
				device->BindBlendFactor(1.0, 1.0, 1.0, 1.0, cmd);
			} break;

			case 4: // additive
			{
				device->BindPipelineState( &psoAdd, cmd ); //Particles
				device->BindBlendFactor(1.0, 1.0, 0.999, 1.0, cmd);
			} break;

			default:
			{
				device->BindPipelineState( &psoAdd, cmd );
				device->BindBlendFactor(1.0, 1.0, 0.999, 1.0, cmd);
			} break;
		}

		gpup_emitter[e].mainPSConstantData.agk_time = AGKTimer();

		// GGMAX 2026-08-07 DEVICE-REMOVAL FIX: this runs INSIDE the transparent render pass
		// (customDraw_Transparent). UpdateBuffer records a CopyBuffer — a copy op inside a
		// D3D12 render pass is an INVALID CALL and removed the device at submit
		// (DXGI_ERROR_INVALID_CALL on Present). DX11 could UpdateSubresource anywhere; the
		// modern in-pass idiom is BindDynamicConstantBuffer — transient upload memory, no
		// copy command recorded. Same change on the per-tile VS constants below.
		device->BindDynamicConstantBuffer( gpup_emitter[e].mainPSConstantData, 1, cmd );

		int numIndices = 0;
		const GPUBuffer* vbs[ 1 ];
		if ( gpup_emitter[e].particles == 32 ) 
		{
			vbs[ 0 ] = &mainVertexBufferObj1;
			device->BindIndexBuffer( &mainIndexBufferObj1, IndexBufferFormat::UINT16, 0, cmd );
			numIndices = mainIndexCountObj1;
		}
		else 
		{
			vbs[ 0 ] = &mainVertexBufferObj0;
			device->BindIndexBuffer( &mainIndexBufferObj0, IndexBufferFormat::UINT16, 0, cmd );
			numIndices = mainIndexCountObj0;
		}

		uint32_t stride = sizeof(GPUP_1024_Vertex); // 3 position + 2 uv
		device->BindVertexBuffers( vbs, 0, 1, &stride, 0, cmd );

		// GGMAX 2026-08-08 (task #122 round 7): forced descriptor re-dirty. The engine's
		// BindResource/BindSampler compare-skip leaves a slot's dirty bit clear when the same
		// object is re-bound — legal only if the previously flushed GPU descriptor table is
		// still intact. The round-6 bisect convicted the gradient_1 (t2) SAMPLE of flipping
		// for seconds at a time with content/sampler/CB/uv all exonerated — a stale-table
		// class. Binding a DECOY object first forces the compare to fail on every slot, so
		// every draw flushes a freshly copied table. A/B lever: pings dying under REBIND 1
		// convicts the stale-descriptor path AND doubles as the workaround.
		if ( gg_gpup_rebind )
		{
			for ( uint32_t s = 0; s <= 5; s++ ) device->BindResource( &texNoiseOrig, s, cmd );
			device->BindSampler( &samplerLinearWrap, 0, cmd );
			device->BindSampler( &samplerLinear, 1, cmd );
			device->BindSampler( &samplerPoint, 2, cmd );
		}

		// bind textures
		device->BindResource(&gpup_emitter[e].texPos[ gpup_emitter[e].currImage ], 0, cmd );
		device->BindResource(&gpup_emitter[e].gradient_1, 2, cmd );
		device->BindResource(&gpup_emitter[e].texSpeed[ gpup_emitter[e].currImage ], 5, cmd );

		device->BindResource(&gpup_emitter[e].imagex, 1, cmd );
		device->BindResource(&gpup_emitter[e].image1, 3, cmd );
		device->BindResource(&texDist2, 4, cmd );

		// bind samplers
		device->BindSampler(&samplerLinear, 0, cmd );
		device->BindSampler(&samplerPoint, 1, cmd );
		device->BindSampler(&samplerLinearWrap, 2, cmd );

		device->BindSampler(&samplerPoint, 1, cmd );

		int ii = gpup_emitter[e].particles / 64;
		if ( gpup_emitter[e].particles == 32 ) ii = 1;
	
		for( int i = 0; i < ii; i++ )
		{
			for( int j = 0; j < ii; j++ )
			{
				gpup_emitter[e].mainVSConstantData.World.m[3][0] = 64.0f * i;
				gpup_emitter[e].mainVSConstantData.World.m[3][1] = 0.0f;
				gpup_emitter[e].mainVSConstantData.World.m[3][2] = 64.0f * j;

				// in-pass safe (see mainPS note above); replaces UpdateBuffer+static CB
				device->BindDynamicConstantBuffer( gpup_emitter[e].mainVSConstantData, 0, cmd );

				device->DrawIndexed( numIndices, 0, 0, cmd );
			}
		}
		device->EventEnd(cmd);
	}
}

void gpup_updatesettings( int enr )
{
	if ( enr < 0 ) return;
	
	// only for files
	gpup_emitter[enr].posConstantData.emittertype = (float) gpup_emitter[enr].emitter_type;
	// ***
		
	float narea = 0.5f + gpup_emitter[enr].narea * 16.0f;
	switch( gpup_emitter[enr].emitter_type )
	{
		case 0:
		{
			gpup_emitter[enr].posConstantData.emittersize.x = gpup_emitter[enr].emitter_size_x * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.y = gpup_emitter[enr].emitter_size_y * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.z = gpup_emitter[enr].emitter_size_z * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.w = 0; 			
		} break;
				
		case 1:
		{
			gpup_emitter[enr].posConstantData.emittersize.x = gpup_emitter[enr].emitter_radius_1 * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.y = gpup_emitter[enr].emitter_radius_2 * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.z = gpup_emitter[enr].emitter_height * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.w = 1.0f; 
		} break;
				
		case 2:
		{
			gpup_emitter[enr].posConstantData.emittersize.x = gpup_emitter[enr].emitter_radius_1 * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.y = gpup_emitter[enr].emitter_radius_2 * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.z = 0.0f;
			gpup_emitter[enr].posConstantData.emittersize.w = 2.0f;
		} break;
				
		case 3:
		{
			gpup_emitter[enr].posConstantData.emittersize.x = (gpup_emitter[enr].emitter_x1 - 0.5f) * 2.0f * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.y = (gpup_emitter[enr].emitter_y1 - 0.5f) * 2.0f * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.z = (gpup_emitter[enr].emitter_z1 - 0.5f) * 2.0f * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emittersize.w = 3.0f;
			
			gpup_emitter[enr].posConstantData.emitterline.x = (gpup_emitter[enr].emitter_x2 - 0.5f) * 2.0f * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emitterline.y = (gpup_emitter[enr].emitter_y2 - 0.5f) * 2.0f * gpup_emitter[enr].varea;
			gpup_emitter[enr].posConstantData.emitterline.z = (gpup_emitter[enr].emitter_z2 - 0.5f) * 2.0f * gpup_emitter[enr].varea;
			
			gpup_emitter[enr].posConstantData.emittersway.x = gpup_emitter[enr].emitter_line_sway_speed;
			gpup_emitter[enr].posConstantData.emittersway.y = gpup_emitter[enr].emitter_line_sway;
			gpup_emitter[enr].posConstantData.emittersway.z = gpup_emitter[enr].emitter_line_sway_size;
			
			gpup_emitter[enr].posConstantData.emitterlinecount.x = gpup_emitter[enr].emitter_lineswirl;
			gpup_emitter[enr].posConstantData.emitterlinecount.y = gpup_emitter[enr].emitter_linebeams;
			gpup_emitter[enr].posConstantData.emitterlinecount.z = gpup_emitter[enr].emitter_linebeamoffset;
		} break;
	}
		
	gpup_emitter[enr].mainPSConstantData.clr.x = gpup_emitter[enr].r;
	gpup_emitter[enr].mainPSConstantData.clr.y = gpup_emitter[enr].g;
	gpup_emitter[enr].mainPSConstantData.clr.z = gpup_emitter[enr].b;
	gpup_emitter[enr].mainPSConstantData.opacity = 1.0f;

	float fFinalSpeedX = gpup_emitter[enr].emitter_speedadjustment_x;// gpup_emitter[enr].emitter_speed_x;
	float fFinalSpeedY = gpup_emitter[enr].emitter_speedadjustment_y;// gpup_emitter[enr].emitter_speed_y;
	float fFinalSpeedZ = gpup_emitter[enr].emitter_speedadjustment_z;// gpup_emitter[enr].emitter_speed_z;

	gpup_emitter[enr].speedConstantData.speedvar.x = (fFinalSpeedX-0.5f)*0.1f;
	gpup_emitter[enr].speedConstantData.speedvar.y = (fFinalSpeedY-0.5f)*0.1f;
	gpup_emitter[enr].speedConstantData.speedvar.z = (fFinalSpeedZ-0.5f)*0.1f;
	gpup_emitter[enr].speedConstantData.speedvar.w = gpup_emitter[enr].emitter_speed_v ; 
	
	gpup_emitter[enr].speedConstantData.field.x = gpup_emitter[enr].field_x*50.0f;
	gpup_emitter[enr].speedConstantData.field.y = gpup_emitter[enr].field_y*50.0f;
	gpup_emitter[enr].speedConstantData.field.z = gpup_emitter[enr].field_z*50.0f;
	gpup_emitter[enr].speedConstantData.field.w = gpup_emitter[enr].field_adhesion*0.1f ;
	
	gpup_emitter[enr].posConstantData.gravity.x = (gpup_emitter[enr].gravity_x-0.5f)/60.0f;
	gpup_emitter[enr].posConstantData.gravity.y = (gpup_emitter[enr].gravity_y-0.5f)/60.0f;
	gpup_emitter[enr].posConstantData.gravity.z = (gpup_emitter[enr].gravity_z-0.5f)/60.0f;
	gpup_emitter[enr].posConstantData.gravity.w = (float) gpup_emitter[enr].emitter_corners ;
	
	gpup_emitter[enr].speedConstantData.gravity.x = (gpup_emitter[enr].gravity_x-0.5f)/60.0f;
	gpup_emitter[enr].speedConstantData.gravity.y = (gpup_emitter[enr].gravity_y-0.5f)/60.0f;
	gpup_emitter[enr].speedConstantData.gravity.z = (gpup_emitter[enr].gravity_z-0.5f)/60.0f;
	gpup_emitter[enr].speedConstantData.gravity.w = gpup_emitter[enr].noise_bias*0.05f ;
	
	gpup_emitter[enr].posConstantData.area.x = gpup_emitter[enr].area;
	gpup_emitter[enr].posConstantData.area.y = gpup_emitter[enr].varea;
	gpup_emitter[enr].posConstantData.area.z = 64;
	
	gpup_emitter[enr].speedConstantData.area.x = gpup_emitter[enr].area;
	gpup_emitter[enr].speedConstantData.area.y = gpup_emitter[enr].varea;
	gpup_emitter[enr].speedConstantData.area.z = 64;
	gpup_emitter[enr].speedConstantData.area.w = narea ; 
	
	gpup_emitter[enr].mainVSConstantData.area.x = gpup_emitter[enr].area;
	gpup_emitter[enr].mainVSConstantData.area.y = gpup_emitter[enr].varea;
	gpup_emitter[enr].mainVSConstantData.area.z = 64;
	
	gpup_emitter[enr].posConstantData.lifespan.x = gpup_emitter[enr].lifespan*60.0f*10.0f;
	gpup_emitter[enr].posConstantData.lifespan.y = gpup_emitter[enr].lifespan_variance;
	
	gpup_emitter[enr].mainVSConstantData.pgrow.x = 0.001f+gpup_emitter[enr].size1*gpup_emitter[enr].sizer;
	gpup_emitter[enr].mainVSConstantData.pgrow.y = 0.001f+gpup_emitter[enr].size2*gpup_emitter[enr].sizer;
	gpup_emitter[enr].mainVSConstantData.pgrow.z = gpup_emitter[enr].particleSize;
	
	gpup_emitter[enr].speedConstantData.autorot.x = gpup_emitter[enr].auto_rot_x - 0.5f;
	gpup_emitter[enr].speedConstantData.autorot.y = gpup_emitter[enr].auto_rot_y - 0.5f;
	gpup_emitter[enr].speedConstantData.autorot.z = gpup_emitter[enr].auto_rot_z - 0.5f;
	gpup_emitter[enr].speedConstantData.autorot.w = gpup_emitter[enr].offset_speed*0.5f ;
		
	int particles = gpup_emitter[enr].particles;
	gpup_emitter[enr].mainVSConstantData.particles.x = (float) particles;
	gpup_emitter[enr].mainVSConstantData.particles.y = (float) (particles*particles);
	gpup_emitter[enr].mainVSConstantData.particles.z = (float) (particles*particles-1);
	gpup_emitter[enr].mainVSConstantData.particles.w = (float) gpup_emitter[enr].facing;
	
	gpup_emitter[enr].posConstantData.particles.x = (float) particles;
	gpup_emitter[enr].posConstantData.particles.y = (float) (particles*particles);
	gpup_emitter[enr].posConstantData.particles.z = (float) (particles*particles-1);
	
	gpup_emitter[enr].speedConstantData.particles.x = (float) particles;
	gpup_emitter[enr].speedConstantData.particles.y = (float) (particles*particles);
	gpup_emitter[enr].speedConstantData.particles.z = (float) (particles*particles-1);
	
	gpup_emitter[enr].mainVSConstantData.image_count.x = (float) (gpup_emitter[enr].image_count+1);
	gpup_emitter[enr].mainVSConstantData.image_count.y = gpup_emitter[enr].image_distortion;
	gpup_emitter[enr].mainVSConstantData.image_count.z = gpup_emitter[enr].image_distortion_speed;
	
	gpup_emitter[enr].mainPSConstantData.image_count.x = (float) (gpup_emitter[enr].image_count+1);
	gpup_emitter[enr].mainPSConstantData.image_count.y = gpup_emitter[enr].image_distortion;
	gpup_emitter[enr].mainPSConstantData.image_count.z = gpup_emitter[enr].image_distortion_speed;
	
	gpup_emitter[enr].posConstantData.pos1.x = gpup_emitter[enr].emitter_x1;
	gpup_emitter[enr].posConstantData.pos1.y = gpup_emitter[enr].emitter_y1;
	gpup_emitter[enr].posConstantData.pos1.z = gpup_emitter[enr].emitter_z1;
	
	gpup_emitter[enr].posConstantData.pos2.x = gpup_emitter[enr].emitter_x2;
	gpup_emitter[enr].posConstantData.pos2.y = gpup_emitter[enr].emitter_y2;
	gpup_emitter[enr].posConstantData.pos2.z = gpup_emitter[enr].emitter_z2;
		
	int bop = 0;
	int subs = gpup_getSubPositionCount( enr );
	for( int i = 0; i < 8; i++ )
	{
		if ( gpup_emitter[enr].subEmitterUsed[i] == 1 )
		{
			gpup_emitter[enr].mainVSConstantData.globalpos[bop].x = gpup_emitter[enr].globalx[i];
			gpup_emitter[enr].mainVSConstantData.globalpos[bop].y = gpup_emitter[enr].globaly[i];
			gpup_emitter[enr].mainVSConstantData.globalpos[bop].z = gpup_emitter[enr].globalz[i];
			gpup_emitter[enr].mainVSConstantData.globalpos[bop].w = (float) subs;

			bop++;
		}
	}
		
	gpup_emitter[enr].mainVSConstantData.globalsize.x = gpup_emitter[enr].globalSize;
	gpup_emitter[enr].mainVSConstantData.globalsize.y = gpup_emitter[enr].globalSize;
	gpup_emitter[enr].mainVSConstantData.globalsize.z = gpup_emitter[enr].globalSize;
	
	gpup_emitter[enr].mainVSConstantData.globalrot.x = 0;// gpup_emitter[enr].globalRotX; set in gpup_setGlobalRotation
	gpup_emitter[enr].mainVSConstantData.globalrot.y = 0;// gpup_emitter[enr].globalRotY;
	gpup_emitter[enr].mainVSConstantData.globalrot.z = 0;// gpup_emitter[enr].globalRotZ;

	gpup_emitter[enr].speedConstantData.reffloor.x = gpup_emitter[enr].floorActive;
	gpup_emitter[enr].speedConstantData.reffloor.y = gpup_emitter[enr].floorHeight;
	gpup_emitter[enr].speedConstantData.reffloor.z = gpup_emitter[enr].sphereRadius;
	gpup_emitter[enr].speedConstantData.reffloor.w = gpup_emitter[enr].bounciness ;
	
	gpup_emitter[enr].speedConstantData.refsphere.x = gpup_emitter[enr].sphereActive;
	gpup_emitter[enr].speedConstantData.refsphere.y = gpup_emitter[enr].sphereX;
	gpup_emitter[enr].speedConstantData.refsphere.z = gpup_emitter[enr].sphereY;
	gpup_emitter[enr].speedConstantData.refsphere.w = gpup_emitter[enr].sphereZ ;
	
	gpup_emitter[enr].posConstantData.localemitter.x = gpup_emitter[enr].emitter_local_x*4.0f-2.0f;
	gpup_emitter[enr].posConstantData.localemitter.y = gpup_emitter[enr].emitter_local_y*4.0f-2.0f;
	gpup_emitter[enr].posConstantData.localemitter.z = gpup_emitter[enr].emitter_local_z*4.0f-2.0f;
}

void gpup_addEmitter( int enr, int selcol )
{	
	GraphicsDevice* device = wiGraphics::GetDevice();

	gpup_emitter[enr].mainVSConstantData.globalsize.x = 1;
	gpup_emitter[enr].mainVSConstantData.globalsize.y = 1;
	gpup_emitter[enr].mainVSConstantData.globalsize.z = 1;
	
	gpup_emitter[enr].mainVSConstantData.globalrot.x = 0;
	gpup_emitter[enr].mainVSConstantData.globalrot.y = 0;
	gpup_emitter[enr].mainVSConstantData.globalrot.z = 0;
	
	gpup_emitter[enr].mainVSConstantData.particles.x = (float) selcol;
	gpup_emitter[enr].mainVSConstantData.particles.y = (float) (selcol*selcol);
	gpup_emitter[enr].mainVSConstantData.particles.z = (float) (selcol*selcol-1);
	gpup_emitter[enr].mainVSConstantData.particles.w = (float) gpup_emitter[enr].facing ;
	
	gpup_emitter[enr].posConstantData.particles.x = (float) selcol;
	gpup_emitter[enr].posConstantData.particles.y = (float) (selcol*selcol);
	gpup_emitter[enr].posConstantData.particles.z = (float) (selcol*selcol-1);
	
	gpup_emitter[enr].speedConstantData.particles.x = (float) selcol;
	gpup_emitter[enr].speedConstantData.particles.y = (float) (selcol*selcol);
	gpup_emitter[enr].speedConstantData.particles.z = (float) (selcol*selcol-1);
		
	gpup_emitter[enr].particles = selcol;
	int particles = selcol;	

	gpup_emitter[enr].currImage = 0;

	// Main Textures
	GPUP_CreateRenderTexture( particles*2, particles*2, &gpup_emitter[enr].texPos[0] );
	GPUP_CreateRenderTexture( particles*2, particles*2, &gpup_emitter[enr].texPos[1] );

	GPUP_CreateRenderTexture( particles*2, particles*2, &gpup_emitter[enr].texSpeed[0] );
	GPUP_CreateRenderTexture( particles*2, particles*2, &gpup_emitter[enr].texSpeed[1] );

	GPUP_CreateRenderTexture( 4096, 64, &gpup_emitter[enr].texNoise );

	// render passes for above textures
	RenderPassDesc renderDesc;
	renderDesc.attachments = {{ RenderPassAttachment::Type::RENDERTARGET, RenderPassAttachment::LoadOp::DONTCARE, gpup_emitter[enr].texPos[0], -1 }};
	device->CreateRenderPass( &renderDesc, &gpup_emitter[enr].renderPassPos[0] );

	renderDesc.attachments = {{ RenderPassAttachment::Type::RENDERTARGET, RenderPassAttachment::LoadOp::DONTCARE, gpup_emitter[enr].texPos[1], -1 }};
	device->CreateRenderPass( &renderDesc, &gpup_emitter[enr].renderPassPos[1] );

	renderDesc.attachments = {{ RenderPassAttachment::Type::RENDERTARGET, RenderPassAttachment::LoadOp::DONTCARE, gpup_emitter[enr].texSpeed[0], -1 }};
	device->CreateRenderPass( &renderDesc, &gpup_emitter[enr].renderPassSpeed[0] );

	renderDesc.attachments = {{ RenderPassAttachment::Type::RENDERTARGET, RenderPassAttachment::LoadOp::DONTCARE, gpup_emitter[enr].texSpeed[1], -1 }};
	device->CreateRenderPass( &renderDesc, &gpup_emitter[enr].renderPassSpeed[1] );

	renderDesc.attachments = {{ RenderPassAttachment::Type::RENDERTARGET, RenderPassAttachment::LoadOp::DONTCARE, gpup_emitter[enr].texNoise, -1 }};
	device->CreateRenderPass( &renderDesc, &gpup_emitter[enr].renderPassNoise );
			
	gpup_emitter[enr].testpos = 0;
	gpup_emitter[enr].emitter_burst_delaycount = 5;
		
	gpup_setBlendmode( enr, gpup_emitter[enr].blendmode );
	gpup_emitter[enr].maxed = 0;
	gpup_settings.gtimer = 0;
	gpup_settings.pauser = 3;
	gpup_updatesettings( enr );

	gpup_emitter[enr].posConstantData.spawnpos.x = -3.0f;
	gpup_emitter[enr].posConstantData.spawnpos.y = -2.0f;
	
	gpup_emitter[enr].speedConstantData.spawnpos.x = -3.0f;
	gpup_emitter[enr].speedConstantData.spawnpos.y = -2.0f;

	gpup_emitter[enr].noiseOff2 = 0.0000152587890625f * Random();
	gpup_emitter[enr].noiseOff1 = 0.0f;
	gpup_emitter[enr].sizer = 1.0f;
	
	noiseConstantData.off.x = gpup_emitter[enr].noiseOff1;
	noiseConstantData.off.y = gpup_emitter[enr].noiseOff2;
	noiseConstantData.off.z = 0;

	gpup_emitter[enr].effectVisible = 1;
}

int gpup_loadEffectFile( const char* fl, float x, float y, float z, float s, float qual, float ps )
{
	// While gpup_init() leaves the system disabled (DX12 port pending), loading an effect
	// would still allocate its 5 renderTex + 3-4 imageTex per emitter — textures nothing can
	// ever draw (gpup_update/gpup_draw early-return on the same flag). 45 emitters on Aztec
	// Game Kit = 149 MB of dead VRAM. Every caller already handles -1.
	if ( !gpu_particles_initialised ) return -1;

	int   tmpint = 0;
	float tmpfloat = 0;

	//gpup_maxeffects
	int enr = -1;
	gpup_settings.emitterCount = 0;
	for( int i = 0; i < gpup_maxeffects; i++ )
	{
		if ( gpup_emitter[i].effectLoaded == 0 && enr == -1 ) enr = i;
		if ( gpup_emitter[i].effectLoaded == 1 ) gpup_settings.emitterCount = gpup_settings.emitterCount + 1;
	}
		
	if ( enr <= -1 ) return -1;
		
	gpup_emitter[enr].globalx[0] = x;
	gpup_emitter[enr].globaly[0] = y;
	gpup_emitter[enr].globalz[0] = z;
		
	gpup_emitter[enr].globalSize = s;
	gpup_emitter[enr].particleSize = ps;
		
	gpup_emitter[enr].subEmitters = 1;
	gpup_emitter[enr].subEmitterUsed[0] = 1;
		
	gpup_emitter[enr].r = 1.0f;
	gpup_emitter[enr].g = 1.0f;
	gpup_emitter[enr].b = 1.0f;

	if ( *fl == 0 ) return -1;
		
	std::string flo = fl;
	flo += ".arx";

	if ( !GetFileExists( flo.c_str() ) ) return -1;
			
	// load the effect
	OpenToRead( 1, flo.c_str() );
				
	gpup_emitter[enr].name = fl;
	AGKDumpString( 1 );
					
	gpup_emitter[enr].x = AGKReadFloat( 1 );
	gpup_emitter[enr].y = AGKReadFloat( 1 );
	gpup_emitter[enr].z = AGKReadFloat( 1 );
	gpup_emitter[enr].scale = AGKReadFloat( 1 );
	int selcol = (int) AGKReadFloat( 1 );
	selcol = (int) floor( selcol * qual );
	if ( selcol < 32 ) selcol = 32;
	if ( selcol > 1024 ) selcol = 1024;
				
	gpup_emitter[enr].fsize = AGKReadFloat( 1 );
	gpup_emitter[enr].frez = ReadInteger( 1 );
				
	gpup_emitter[enr].emiton = ReadInteger( 1 );

	gpup_emitter[enr].area = AGKReadFloat( 1 );
	gpup_emitter[enr].varea = AGKReadFloat( 1 );
	gpup_emitter[enr].narea = AGKReadFloat( 1 );
			
	gpup_emitter[enr].blendmode = ReadInteger( 1 );
	gpup_emitter[enr].col = ReadInteger( 1 );
			
	gpup_emitter[enr].layer_count = ReadInteger( 1 );

	gpup_emitter[enr].field_adhesion = AGKReadFloat( 1 );
	gpup_emitter[enr].field_x = AGKReadFloat( 1 );
	gpup_emitter[enr].field_y = AGKReadFloat( 1 );
	gpup_emitter[enr].field_z = AGKReadFloat( 1 );
			
	gpup_emitter[enr].image_count = ReadInteger( 1 );
	AGKDumpString( 1 );
	AGKDumpString( 1 );
	AGKDumpString( 1 );
	AGKDumpString( 1 );
	AGKDumpString( 1 );
	AGKDumpString( 1 );
	AGKDumpString( 1 );
				
	gpup_emitter[enr].noise_adhesion = AGKReadFloat( 1 );
	gpup_emitter[enr].offset_speed = AGKReadFloat( 1 );
	gpup_emitter[enr].auto_rot_x = AGKReadFloat(1);
	gpup_emitter[enr].auto_rot_y = AGKReadFloat(1);
	gpup_emitter[enr].auto_rot_z = AGKReadFloat(1);
	gpup_emitter[enr].noise_bias = AGKReadFloat( 1 );
				
	gpup_emitter[enr].gravity_x = AGKReadFloat( 1 );
	gpup_emitter[enr].gravity_y = AGKReadFloat( 1 );
	gpup_emitter[enr].gravity_z = AGKReadFloat( 1 );
				
	gpup_emitter[enr].lifespan = AGKReadFloat( 1 );
	gpup_emitter[enr].lifespan_variance = AGKReadFloat( 1 );
	gpup_emitter[enr].maxed = AGKReadFloat( 1 );
				
	gpup_emitter[enr].emitter_type = ReadInteger( 1 );
	gpup_emitter[enr].emitter_local_x = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_local_y = AGKReadFloat( 1 );
	gpup_emitter[enr].emitter_local_z = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_size_x = AGKReadFloat( 1 );
	gpup_emitter[enr].emitter_size_y = AGKReadFloat( 1 );
	gpup_emitter[enr].emitter_size_z = AGKReadFloat( 1 );
	gpup_emitter[enr].emitter_radius_1 = AGKReadFloat( 1 );
	gpup_emitter[enr].emitter_radius_2 = AGKReadFloat( 1 );
	gpup_emitter[enr].emitter_height = AGKReadFloat( 1 );
	gpup_emitter[enr].emitter_x1 = AGKReadFloat( 1 );
	gpup_emitter[enr].emitter_y1 = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_z1 = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_x2 = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_y2 = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_z2 = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_rotation_x = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_rotation_y = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_rotation_z = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_speed_x = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_speed_y = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_speed_z = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_speed_v = AGKReadFloat( 1 ); 
				
	gpup_emitter[enr].emitter_slow_mode = ReadInteger( 1 ); 
	gpup_emitter[enr].emitter_amount = AGKReadFloat( 1 ); 
				
	gpup_emitter[enr].emitter_burst_mode = ReadInteger( 1 ); 
	gpup_emitter[enr].emitter_burst_auto = ReadInteger( 1 ); 
	gpup_emitter[enr].emitter_burst_delay = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_burst_frames = ReadInteger( 1 ); 
	gpup_emitter[enr].emitter_burst_fire = ReadInteger( 1 ); 
	gpup_emitter[enr].emitter_burst_delaycount = (float) ReadInteger( 1 ); 
				
	gpup_emitter[enr].size1 = AGKReadFloat( 1 ); 
	gpup_emitter[enr].size2 = AGKReadFloat( 1 ); 
	gpup_emitter[enr].rotation = AGKReadFloat( 1 ); 
	gpup_emitter[enr].rotation_variance = AGKReadFloat( 1 ); 
				
	gpup_emitter[enr].emitter_wire = ReadInteger( 1 );
	gpup_emitter[enr].facing = ReadInteger( 1 );
				
	gpup_emitter[enr].emitter_auto_rotation_x = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_auto_rotation_y = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_auto_rotation_z = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_auto_movement = ReadInteger( 1 );
				
	gpup_emitter[enr].emitter_line_sway_speed = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_line_sway = AGKReadFloat( 1 ); 
	gpup_emitter[enr].emitter_line_sway_size = AGKReadFloat( 1 ); 
				
	gpup_emitter[enr].image_distortion = AGKReadFloat( 1 );
	gpup_emitter[enr].image_distortion_speed = AGKReadFloat( 1 );
				
	tmpint = ReadInteger( 1 );
	if ( tmpint > 900 ) tmpint = 0;
	gpup_emitter[enr].emitter_corners = tmpint;
	tmpint = ReadInteger( 1 );
	if ( tmpint > 900 ) tmpint = 0;
	gpup_emitter[enr].emitter_moving = tmpint;
	tmpint = ReadInteger( 1 );
	if ( tmpint > 900 ) tmpint = 0;
	gpup_emitter[enr].ani_x = tmpint;
	tmpint = ReadInteger( 1 );
	if ( tmpint > 900 ) tmpint = 0;
	gpup_emitter[enr].ani_y = tmpint;
	tmpint = ReadInteger( 1 );
	if ( tmpint > 900 ) tmpint = 0;
	gpup_emitter[enr].ani_s = tmpint;
	tmpint = ReadInteger( 1 );
	if ( tmpint > 900 ) tmpint = 0;
	gpup_emitter[enr].smooth_blending = tmpint;
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
				
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
	tmpint = ReadInteger( 1 );
				
	tmpfloat = AGKReadFloat( 1 );
	if ( tmpfloat > 900 ) tmpfloat = 0.5f;
	gpup_emitter[enr].emitter_moving_speed = tmpfloat;
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
							 
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
	tmpfloat = AGKReadFloat( 1 );
				
	for( int i = 0; i <= gpup_emitter[enr].layer_count; i++ )
	{
		int reflayer = 0;
		tmpint = ReadInteger( 1 );
		if ( tmpint == 1 ) reflayer = 1;
				
		float local_x = AGKReadFloat( 1 );
		float local_y = AGKReadFloat( 1 );
		float local_z = AGKReadFloat( 1 );
					
		float rotation_x = AGKReadFloat( 1 );
		float rotation_y = AGKReadFloat( 1 );
		float rotation_z = AGKReadFloat( 1 );
					
		tmpfloat = AGKReadFloat( 1 );
		tmpfloat = AGKReadFloat( 1 );
					
		float bounciness = AGKReadFloat( 1 );
					
		for( int j = 0; j < 11; j++ ) tmpfloat = AGKReadFloat( 1 );
					
		float radius_1 = AGKReadFloat( 1 );
		float radius_2 = AGKReadFloat( 1 );
					
		tmpfloat = AGKReadFloat( 1 );
		tmpfloat = AGKReadFloat( 1 );					
					
		if ( reflayer == 1 )
		{
			gpup_emitter[enr].bounciness = bounciness;
			gpup_emitter[enr].floorActive = rotation_x;
			gpup_emitter[enr].sphereActive = rotation_y;
			gpup_emitter[enr].sphereRadius = radius_1;
			gpup_emitter[enr].floorHeight = radius_2;
			gpup_emitter[enr].sphereX = local_x;
			gpup_emitter[enr].sphereY = local_y;
			gpup_emitter[enr].sphereZ = local_z;
		}				
	}
				
	AGKCloseFile( 1 );

	// copy original speed vector into adjustable vector (can now be altered in realtime)
	gpup_emitter[enr].emitter_speedadjustment_x = gpup_emitter[enr].emitter_speed_x;
	gpup_emitter[enr].emitter_speedadjustment_y = gpup_emitter[enr].emitter_speed_y;
	gpup_emitter[enr].emitter_speedadjustment_z = gpup_emitter[enr].emitter_speed_z;
				
	gpup_addEmitter( enr, selcol );
				
	flo = fl; flo += "_g.png";
	if ( !GetFileExists(flo.c_str()) ) return -1;
	
	GPUP_LoadTexture( flo.c_str(), &gpup_emitter[enr].gradient_1 );
				
	if ( gpup_emitter[enr].ani_x > 0 )
	{
		flo = fl; flo += "_s1.png";
		if ( !GetFileExists(flo.c_str()) ) return -1;

		GPUP_LoadTexture( flo.c_str(), &gpup_emitter[enr].image1 );
	}
				
	flo = fl; flo += "_sx.png";
	if ( !GetFileExists(flo.c_str()) ) return -1;

	GPUP_LoadTexture( flo.c_str(), &gpup_emitter[enr].imagex );
				
	gpup_emitter[enr].mainPSConstantData.tile.x = (float) gpup_emitter[enr].ani_x;
	gpup_emitter[enr].mainPSConstantData.tile.y = (float) gpup_emitter[enr].ani_y;
	gpup_emitter[enr].mainPSConstantData.tile.z = (float) gpup_emitter[enr].ani_s;
	gpup_emitter[enr].mainPSConstantData.tile.w = (float) gpup_emitter[enr].smooth_blending ;
	
	gpup_emitter[enr].mainVSConstantData.tilex.x = (float) gpup_emitter[enr].ani_x;
	gpup_emitter[enr].mainVSConstantData.tilex.y = (float) gpup_emitter[enr].ani_y;
	gpup_emitter[enr].mainVSConstantData.tilex.z = (float) gpup_emitter[enr].ani_s;
	gpup_emitter[enr].mainVSConstantData.tilex.w = (float) gpup_emitter[enr].smooth_blending ;
				
	gpup_emitter[enr].effectLoaded = 1;
	gpup_emitter[enr].pivotMoved = 0;
	gpup_settings.emitterCount = gpup_settings.emitterCount + 1;
				
	gpup_emitter[enr].mainVSConstantData.image_count.x = (float) (gpup_emitter[enr].image_count+1);
	gpup_emitter[enr].mainVSConstantData.image_count.y = 0;
	gpup_emitter[enr].mainVSConstantData.image_count.z = 0;
	
	gpup_emitter[enr].mainPSConstantData.image_count.x = (float) (gpup_emitter[enr].image_count+1);
	gpup_emitter[enr].mainPSConstantData.image_count.y = 0;
	gpup_emitter[enr].mainPSConstantData.image_count.z = 0;
				
	gpup_setBlendmode( enr, gpup_emitter[enr].blendmode );
				
	gpup_emitter[enr].emitter_burst_delaycount = gpup_emitter[enr].emitter_burst_delay*550.0f + 22.0f;
				
	flo = fl; flo += "_r.png";
	if ( !GetFileExists(flo.c_str()) ) return -1;
	
	GPUP_LoadTexture( flo.c_str(), &gpup_emitter[enr].t_field );
					
	gpup_emitter[enr].testpos = 0;
	gpup_emitter[enr].spawnint = 0;
	gpup_emitter[enr].noiseZaehler = 0;
	gpup_settings.gtimer = 0;

	gpup_emitter[enr].emitter_animation_speed = 1.0f;

	gpup_updatesettings( enr );
		
	return enr;
}

void gpup_spawnit( int enr, int spawnint )
{
	int pr = gpup_emitter[enr].particles;
	pr = pr * pr;
	
	spawnint = spawnint * gpup_getSubPositionCount( enr );
	
	if ( spawnint > 0 )
	{
		gpup_emitter[enr].activeTimer = 1.12f;
		gpup_emitter[enr].testpos = gpup_emitter[enr].testpos + spawnint;
		if ( gpup_emitter[enr].testpos > pr-1 ) gpup_emitter[enr].testpos = gpup_emitter[enr].testpos - pr;
		
		gpup_emitter[enr].posConstantData.spawnpos.x = (float) gpup_emitter[enr].testpos;
		gpup_emitter[enr].posConstantData.spawnpos.y = (float) (gpup_emitter[enr].testpos+spawnint-1);
		
		gpup_emitter[enr].speedConstantData.spawnpos.x = (float) gpup_emitter[enr].testpos;
		gpup_emitter[enr].speedConstantData.spawnpos.y = (float) (gpup_emitter[enr].testpos+spawnint-1);
	}
	else
	{
		gpup_emitter[enr].posConstantData.spawnpos.x = -1000;
		gpup_emitter[enr].posConstantData.spawnpos.y = -999;
		
		gpup_emitter[enr].speedConstantData.spawnpos.x = -1000;
		gpup_emitter[enr].speedConstantData.spawnpos.y = -999;
	}
}


void  gpup_doit( int enr, CommandList cmd )
{
	GraphicsDevice* device = wiGraphics::GetDevice();
	// GGMAX 2026-08-07 cadence counters (task #121) — see GPUP_DUMP
	gg_gpup_doit_calls++;
	gg_gpup_time_sum += gpup_settings.time;
	if (gpup_settings.time > gg_gpup_max_time) gg_gpup_max_time = gpup_settings.time;
	float emitter_time = gpup_settings.time * gpup_emitter[enr].emitter_animation_speed;

	gpup_emitter[enr].speedConstantData.warp = emitter_time;
	gpup_emitter[enr].posConstantData.warp = emitter_time;
		
	if ( gpup_settings.pauser > 0 ) gpup_settings.pauser = gpup_settings.pauser - 1;
	
	if ( gpup_settings.simulOn == 1 )
	{		
		if ( gpup_emitter[enr].noiseDir == 0 )
		{
			gpup_emitter[enr].noiseZaehler = gpup_emitter[enr].noiseZaehler + gpup_emitter[enr].offset_speed * 2.5f * emitter_time;
			if ( gpup_emitter[enr].noiseZaehler > 360.0f )
			{
				gpup_emitter[enr].noiseZaehler = 360.0f;
				gpup_emitter[enr].noiseDir = 1;
				gpup_emitter[enr].noiseOff1 = 0.0000152587890625f * Random();
			}
		}
		else
		{
			gpup_emitter[enr].noiseZaehler = gpup_emitter[enr].noiseZaehler - gpup_emitter[enr].offset_speed * 2.5f * emitter_time;
			if ( gpup_emitter[enr].noiseZaehler < 0.0f )
			{
				gpup_emitter[enr].noiseZaehler = 0.0f;
				gpup_emitter[enr].noiseDir = 0;
				gpup_emitter[enr].noiseOff2 = 0.0000152587890625f * Random();
			}
		}

		noiseConstantData.off.x = gpup_emitter[enr].noiseOff1;
		noiseConstantData.off.y = gpup_emitter[enr].noiseOff2;
		noiseConstantData.off.z = gpup_emitter[enr].noiseZaehler/360.0f;

		// GGMAX 2026-08-08 ★ ROOT CAUSE of task #122 "the ping" — the shared-CB copy RACE.
		// The DX11 original updated ONE shared constant buffer per pass type via
		// UpdateSubresource, whose D3D11 implicit hazard tracking serialized every
		// copy→draw→copy→draw chain automatically. The port kept the pattern with
		// UpdateBuffer, which in Wicked DX12 is AllocateGPU + CopyBuffer and — per its own
		// header comment — "appropriate synchronization is expected" from the CALLER. There
		// is none here, and D3D12 buffers get implicit state promotion, so no barrier and no
		// validation error exist anywhere in the chain: the GPU freely overlaps emitter B's
		// copy with emitter A's still-running sim pass. Every pass could therefore read a
		// LATER emitter's constants — including their SPAWN WINDOW, so each pool consumed up
		// to nine foreign windows per tick (~10x the intended churn, measured 2275 slots/s vs
		// the 227/s cursor). The pool degenerates into one age-synchronized cohort whose
		// simultaneous death/rebirth is the user-visible whole-cloud "ping" every ~lifespan;
		// the same race through speedConstants produced the measured cloud-wide velocity
		// kicks. Fix: per-call transient upload CBs (BindDynamicConstantBuffer) — no shared
		// destination, no copy, no race — the exact pattern the draw path has used since the
		// 08-07 device-removal fix. The old shared buffers stay allocated but unused.
		device->BindDynamicConstantBuffer( noiseConstantData, 0, cmd );
		device->BindPipelineState( &psoNoise, cmd );
		device->BindResource(&texNoiseOrig, 0, cmd );
		GPUParticlesDrawQuad( &gpup_emitter[enr].renderPassNoise, cmd );

		// emittance stats
		int buf = gpup_emitter[enr].particles * gpup_emitter[enr].particles;
		float lif = gpup_emitter[enr].lifespan * 60.0f * 10.0f;
		float per = buf / lif;
		gpup_emitter[enr].maxed = per;
				
		if ( gpup_emitter[enr].emitter_burst_mode == 0 )
		{
			//PE: Looping on, should respect emitter_burst_auto. Just like in the particle editor.
			//PE: https://github.com/TheGameCreators/GameGuruRepo/issues/5929#issuecomment-2740317619
			if (gpup_emitter[enr].emitter_burst_auto == 1 && gpup_emitter[enr].emitterActive == 1)
			{
				gpup_emitter[enr].spawnint = 0;
				if (gpup_emitter[enr].emitter_burst_fire > 0)
				{
					gpup_emitter[enr].spawnint = buf * 0.9f * gpup_emitter[enr].emitter_amount;
					gpup_emitter[enr].emitter_burst_fire = gpup_emitter[enr].emitter_burst_fire - 1;
					gpup_settings.pauser = 0; //PE: Fire it right now.
				}
				if (gpup_emitter[enr].emitter_burst_auto == 1 && gpup_emitter[enr].emitterActive == 1)
				{
					gpup_emitter[enr].emitter_burst_delaycount = gpup_emitter[enr].emitter_burst_delaycount + 1.0f * emitter_time;
					if (gpup_emitter[enr].emitter_burst_delaycount > (gpup_emitter[enr].emitter_burst_delay * 550.0f + 30.0f))
					{
						gpup_emitter[enr].spawnint = buf * 0.9f * gpup_emitter[enr].emitter_amount;
						gpup_emitter[enr].emitter_burst_delaycount = 0;
						gpup_emitter[enr].emitter_burst_fire = 1;
					}
				}
			}
			else
			{
				float spawnamount = gpup_emitter[enr].emitter_amount * gpup_emitter[enr].maxed * emitter_time;
				if (gpup_emitter[enr].emitter_slow_mode == 1) spawnamount = spawnamount / 16.0f;
				gpup_emitter[enr].spawnint = spawnamount;
				if (spawnamount < 1)
				{
					gpup_emitter[enr].spawnCount = gpup_emitter[enr].spawnCount + 1;
					if (gpup_emitter[enr].spawnCount > (1.0f / spawnamount))
					{
						gpup_emitter[enr].spawnint = 1;
						gpup_emitter[enr].spawnCount = 0;
					}
				}
			}
		}
		else
		{
			//PE: Looping off , so only single burst here.
			gpup_emitter[enr].spawnint = 0;
			if ( gpup_emitter[enr].emitter_burst_fire > 0 )
			{
				gpup_emitter[enr].spawnint = buf * 0.9f * gpup_emitter[enr].emitter_amount;
				gpup_emitter[enr].emitter_burst_fire = gpup_emitter[enr].emitter_burst_fire - 1;
				gpup_settings.pauser = 0; //PE: Fire it right now.
			}
			//PE: Moved so we only trigger one explosion when looping off. moved to above when looping on.
			/*
			if ( gpup_emitter[enr].emitter_burst_auto == 1 && gpup_emitter[enr].emitterActive == 1 )
			{
				gpup_emitter[enr].emitter_burst_delaycount = gpup_emitter[enr].emitter_burst_delaycount + 1.0f * emitter_time;
				if ( gpup_emitter[enr].emitter_burst_delaycount > (gpup_emitter[enr].emitter_burst_delay * 550.0f + 30.0f) )
				{
					gpup_emitter[enr].spawnint = buf * 0.9f * gpup_emitter[enr].emitter_amount;
					gpup_emitter[enr].emitter_burst_delaycount = 0;
					gpup_emitter[enr].emitter_burst_fire = 1;
				}
			}
			*/
		}
				
		if ( gpup_emitter[enr].activeTimer > 0.0f )
		{
			if ( gpup_settings.split == 0 ) gpup_settings.activeEffects1 = gpup_settings.activeEffects1 + 1;
			if ( gpup_settings.split == 1 ) gpup_settings.activeEffects2 = gpup_settings.activeEffects2 + 1;
			
			if ( gpup_emitter[enr].pivotMoved > 0 )
			{
				float s = gpup_emitter[enr].globalSize;
				float newX = (gpup_emitter[enr].globalx[0] - gpup_emitter[enr].newX) / s;
				float newY = (gpup_emitter[enr].globaly[0] - gpup_emitter[enr].newY) / s;
				float newZ = (gpup_emitter[enr].globalz[0] - gpup_emitter[enr].newZ) / s;

				gpup_emitter[enr].posConstantData.moveit.x = newX;
				gpup_emitter[enr].posConstantData.moveit.y = newY;
				gpup_emitter[enr].posConstantData.moveit.z = newZ;

				gpup_emitter[enr].pivotMoved = 0;
				gpup_emitter[enr].globalx[0] = gpup_emitter[enr].newX;
				gpup_emitter[enr].globaly[0] = gpup_emitter[enr].newY;
				gpup_emitter[enr].globalz[0] = gpup_emitter[enr].newZ;
				
				gpup_emitter[enr].mainVSConstantData.globalpos[0].x = gpup_emitter[enr].globalx[0];
				gpup_emitter[enr].mainVSConstantData.globalpos[0].y = gpup_emitter[enr].globaly[0];
				gpup_emitter[enr].mainVSConstantData.globalpos[0].z = gpup_emitter[enr].globalz[0];
				gpup_emitter[enr].mainVSConstantData.globalpos[0].w = (float) gpup_getSubPositionCount(enr) ;
			}

			int currImage = gpup_emitter[enr].currImage;
			int nextImage = 1 - currImage;

			// render speed image
			// GGMAX 2026-08-08: shared-CB race fix (task #122) — see the noise pass comment
			device->BindDynamicConstantBuffer( gpup_emitter[enr].speedConstantData, 0, cmd );
			device->BindPipelineState( &psoSpeed, cmd );
			device->BindResource(&gpup_emitter[enr].texPos[currImage], 0, cmd );
			device->BindResource(&gpup_emitter[enr].texSpeed[currImage], 1, cmd );
			device->BindResource(&gpup_emitter[enr].texNoise, 2, cmd );
			device->BindResource(&gpup_emitter[enr].t_field, 3, cmd );
			GPUParticlesDrawQuad( &gpup_emitter[enr].renderPassSpeed[nextImage], cmd );

			gpup_emitter[enr].posConstantData.agk_time = AGKTimer();
			
			// render pos image
			// GGMAX 2026-08-08: shared-CB race fix (task #122) — see the noise pass comment
			device->BindDynamicConstantBuffer( gpup_emitter[enr].posConstantData, 0, cmd );
			device->BindPipelineState( &psoPos, cmd );
			device->BindResource(&gpup_emitter[enr].texPos[currImage], 0, cmd );
			device->BindResource(&gpup_emitter[enr].texSpeed[nextImage], 1, cmd );
			GPUParticlesDrawQuad( &gpup_emitter[enr].renderPassPos[nextImage], cmd );
				
			gpup_emitter[enr].currImage = nextImage;
			
			gpup_emitter[enr].posConstantData.moveit.x = 0;
			gpup_emitter[enr].posConstantData.moveit.y = 0;
			gpup_emitter[enr].posConstantData.moveit.z = 0;
		}
			
		gpup_emitter[enr].posConstantData.spawnpos.x = -3.0f;
		gpup_emitter[enr].posConstantData.spawnpos.y = -2.0f;
		
		gpup_emitter[enr].speedConstantData.spawnpos.x = -3.0f;
		gpup_emitter[enr].speedConstantData.spawnpos.y = -2.0f;
		
		if ( gpup_emitter[enr].emiton == 1 || gpup_emitter[enr].emitter_burst_fire > 0 )
		{
			if ( gpup_settings.pauser < 1 && gpup_emitter[enr].emitterActive == 1 ) gpup_spawnit( enr, (int) gpup_emitter[enr].spawnint );
		}

		// GGMAX 2026-08-08 (tasks #120/#121 round 3): sn is the GPU hash SEED for spawn
		// velocity/rotation randomness, and it grows ~10.4/s forever (DX11 wrapped only at
		// 1e9). The shader hash is frac(sin(dot(seed, primes~285x))*75633): at sn≈32k
		// (≈50 min of runtime) the dot reaches 8-9 MILLION, where fp32 ULP is 0.5 — the
		// mod-2π inside sin() collapses to a handful of distinct values, spawn velocities
		// degenerate into a few coherent jets, and the steam congeals into the
		// user-reported whole-screen white-out. Proven by the level-reload experiment:
		// FRESH emitters inheriting the old clock are instantly broken; GPUP_SHOW 0
		// restores a normal scene. Mid-degradation (~5-30 min) reads as "bigger, moves
		// more, glitches" — the residue of the user's earlier report. DX11 carries the
		// same latent code but sessions rarely idle 50+ min at a particle view (and its
		// fxc/SM5 sin() path may mask it longer). Fix: the accumulator stays untouched;
		// the HASH sees a small cyclic seed. Period 256 ≈ 24.6s macro-repeat in
		// turbulence seeding — imperceptible against ~4s particle lifespans.
		const float gg_hash_seed = fmodf( gpup_settings.sn, 256.0f );
		gpup_emitter[enr].posConstantData.rnd = gg_hash_seed;

		gpup_emitter[enr].speedConstantData.rnd = gg_hash_seed;
		gpup_emitter[enr].speedConstantData.rnd2 = Random2()/65536.0f;

		gpup_emitter[enr].mainVSConstantData.rota.x = gpup_emitter[enr].rotation-0.5f;
		gpup_emitter[enr].mainVSConstantData.rota.y = gpup_emitter[enr].rotation_variance;
		// GGMAX 2026-08-08 round 2 (task #121, USER-REPORTED "positions shift / whole new
		// shape every so often"): rota.z is NOT a hash seed — MainVS uses it as a MULTIPLIER
		// on the per-quad rotation angle (ang = rota.b*(rota.r + vr*rota.g); mode 3 rotates a
		// POSITION offset by it). The fmod above made it ramp for 24.6s then SNAP to zero —
		// every element re-oriented/re-positioned simultaneously, sometimes reading as a new
		// particle shape. A continuous-phase consumer must get the RAW clock (DX11-verbatim:
		// sn grows forever, wraps at 1e9 ≈ every 3 years; fp32 angle quantization at large sn
		// is sub-visible — ULP(32k) ≈ 0.002 rad — and DX11 shipped years of it). The fmod
		// stays ONLY on the sim-shader rnd seeds above, which are pure frac(sin(dot())) hash
		// inputs where magnitude causes fp32 collapse and a wrap is invisible (the hash
		// stream decorrelates every tick regardless).
		gpup_emitter[enr].mainVSConstantData.rota.z = gpup_settings.sn;

		float rotX = gpup_emitter[enr].emitter_rotation_x * 6.2831853f + gpup_settings.rotsn * (gpup_emitter[enr].emitter_auto_rotation_x - 0.5f);
		float rotY = gpup_emitter[enr].emitter_rotation_y * 6.2831853f + gpup_settings.rotsn * (gpup_emitter[enr].emitter_auto_rotation_y - 0.5f);
		float rotZ = gpup_emitter[enr].emitter_rotation_z * 6.2831853f + gpup_settings.rotsn * (gpup_emitter[enr].emitter_auto_rotation_z - 0.5f);

		gpup_emitter[enr].posConstantData.emitterrotation.x = rotX;
		gpup_emitter[enr].posConstantData.emitterrotation.y = rotY;
		gpup_emitter[enr].posConstantData.emitterrotation.z = rotZ;
		gpup_emitter[enr].posConstantData.emitterrotation.w = (float) gpup_emitter[enr].emitter_wire ; 
		
		float lifedrain = 0.998f / (gpup_emitter[enr].lifespan * 60 * 10);
		gpup_emitter[enr].activeTimer = gpup_emitter[enr].activeTimer - lifedrain;
		if ( gpup_emitter[enr].activeTimer < 0.0f ) gpup_emitter[enr].activeTimer = 0.0f;
	}
}

// initialize the system
// GGMAX 2.71: false = init runs identically but writes no gpup_trace.txt (standalones
// run producelogfiles=0 — no trace droppings in players' folders; set via GGSetDiagTraceFiles,
// which GetSetupIniEarly calls before this init runs)
bool gg_gpup_trace_file = true;

int gpup_init()
{
	if ( gpu_particles_initialised ) return 1;
	gpu_particles_initialised = 1;

	GraphicsDevice* device = wiGraphics::GetDevice();

	AGKTimerInit();
	RandomInit();
	Random2Init();

	gpup_settings.simulOn = 1;
	gpup_settings.gtimer = 0;
	gpup_settings.spawnCount = 0;

	// GGMAX 2026-08-07: RE-ENABLED (task #118 — the legacy .arx system carries every entity
	// Particles marker, e.g. the spotshadowtest steam columns; nothing else can, the WPE
	// corpus has no smoke_thick/.arx equivalents). The Feb-17 disable ("CreateBuffer with
	// initial data crashes — staging allocator returns null mapped_data", commit 80c936f5)
	// dated from the raw API-migration window; the CreateBuffer-with-initial-data path has
	// since been the workhorse of every system in the engine (deltas 1.1-2.10) and the buffer
	// descs below are plain DEFAULT-usage IB/VB/CB creates from compile-time arrays. If this
	// ever crashes again the CrashLogger's symbolized walk will name it — do not re-disable
	// wholesale, root-cause it.
	wi::backlog::post("GPU Particles (gpup/.arx): initialising");

	// GGMAX 2026-08-07: gpup_trace.txt — fopen/fflush per line so the evidence survives a
	// crash (wi::backlog buffers and loses its tail; the first re-enable run crashed at the
	// first gpup draw with only a lone D3D12CreateVersionedRootSignatureDeserializer error
	// flushed, and this trace exists to name the failing stage precisely). The Feb-era commit
	// 80c936f5 also removed ALL return-value checking from this function — restored below:
	// any shader/PSO failure now disables the whole system cleanly instead of letting an
	// empty PipelineState reach BindPipelineState (null internal_state -> AV in pso_validate).
	FILE* gg_trace = nullptr;
	if (gg_gpup_trace_file) gg_trace = GGDiagFopen("gpup_trace.txt", "w");
	#define GPUP_TRACE(...) { if (gg_trace) { fprintf(gg_trace, __VA_ARGS__); fprintf(gg_trace, "\n"); fflush(gg_trace); } }
	#define GPUP_FAIL(msg) { GPUP_TRACE("FAIL: %s", msg); if (gg_trace) fclose(gg_trace); \
		wi::backlog::post(std::string("GPU Particles DISABLED: ") + msg, wi::backlog::LogLevel::Error); \
		gpu_particles_initialised = 0; return -1; }

	// Load GPU particle shaders
	bool bShadersOK = true;
	struct { ShaderStage stage; Shader* sh; const char* file; } gg_shaders[] = {
		{ ShaderStage::VS, &shaderMainVS,       "GPUP_MainVS.cso" },
		{ ShaderStage::PS, &shaderMainPS,       "GPUP_MainPS.cso" },
		{ ShaderStage::VS, &shaderQuadVS,       "GPUP_QuadVS.cso" },
		{ ShaderStage::PS, &shaderQuadDefaultPS,"QuadDefaultPS.cso" },
		{ ShaderStage::PS, &shaderNoisePS,      "GPUP_NoisePS.cso" },
		{ ShaderStage::PS, &shaderSpeedPS,      "GPUP_SpeedPS.cso" },
		{ ShaderStage::PS, &shaderPosPS,        "GPUP_PosPS.cso" },
	};
	for (auto& s : gg_shaders)
	{
		bool ok = wiRenderer::LoadShader(s.stage, *s.sh, s.file);
		GPUP_TRACE("LoadShader %-20s ok=%d valid=%d", s.file, ok ? 1 : 0, s.sh->IsValid() ? 1 : 0);
		if (!ok || !s.sh->IsValid()) bShadersOK = false;
	}
	if (!bShadersOK) GPUP_FAIL("one or more shaders failed to load (see lines above)");

	// images
	if ( !GetFileExists("Files/effectbank/common/noise64.png") ) return -1;
	GPUP_LoadTexture( "Files/effectbank/common/noise64.png", &texNoiseOrig );

	if ( !GetFileExists("Files/effectbank/common/dist2.png") ) return -1;
	GPUP_LoadTexture( "Files/effectbank/common/dist2.png", &texDist2 );

	// pipeline state
	// GGMAX 2026-08-07 THE gpup CRASH FIX: these MUST be static. PipelineStateDesc stores
	// POINTERS to the state objects and modern Wicked defers the driver PSO compile to the
	// first draw (pso_validate) — in particular the input layout's SemanticName is kept as a
	// c_str() into THIS InputLayout (wiGraphicsDevice_DX12.cpp:4865). As stack locals they
	// died when gpup_init returned, the lazy compile read dangling "POSITION"/"UV" strings,
	// CreatePipelineState failed and SetPipelineState(nullptr) AV'd in d3d12.dll (0x18c).
	// Engine PSOs never hit this because wiRenderer's layouts are globals; DX11-era Wicked
	// created PSOs eagerly so the original stack locals were correct back then.
	static RasterizerState rasterDesc = {};
	rasterDesc.fill_mode = FillMode::SOLID;
	rasterDesc.cull_mode = CullMode::NONE;
	rasterDesc.front_counter_clockwise = true;
	rasterDesc.depth_bias = 0;
	rasterDesc.depth_bias_clamp = 0;
	rasterDesc.slope_scaled_depth_bias = 0;
	rasterDesc.depth_clip_enable = false;
	rasterDesc.multisample_enable = false;
	rasterDesc.antialiased_line_enable = false;
	
	static DepthStencilState depthDesc = {};
	depthDesc.depth_enable = true;
	depthDesc.depth_write_mask = DepthWriteMask::ZERO;
	depthDesc.depth_func = ComparisonFunc::GREATER_EQUAL;
	depthDesc.stencil_enable = false;

	static BlendState blendDesc = {};
	blendDesc.render_target[0].blend_enable = true;
	blendDesc.render_target[0].src_blend = Blend::SRC_ALPHA;
	blendDesc.render_target[0].dest_blend = Blend::INV_SRC_ALPHA;
	blendDesc.render_target[0].blend_op = BlendOp::ADD;
	blendDesc.render_target[0].src_blend_alpha = Blend::ONE;
	blendDesc.render_target[0].dest_blend_alpha = Blend::INV_SRC_ALPHA;
	blendDesc.render_target[0].blend_op_alpha = BlendOp::ADD;
	blendDesc.render_target[0].render_target_write_mask = ColorWrite::ENABLE_ALL;
	blendDesc.independent_blend_enable = false;
	
	// input layout (static — SemanticName c_str()s are referenced at first-draw PSO compile)
	static InputLayout layoutDesc;
	layoutDesc.elements =
	{
		{ "POSITION", 0, wiGraphics::Format::R32G32B32_FLOAT, 0, 0, InputClassification::PER_VERTEX_DATA },
		{ "UV",       0, wiGraphics::Format::R32G32_FLOAT,    0, 12, InputClassification::PER_VERTEX_DATA },
	};
	
	// pipeline state objects
	PipelineStateDesc desc = {};
	desc.vs = &shaderMainVS;
	desc.ps = &shaderMainPS;

	// alpha
	desc.il = &layoutDesc;
	desc.pt = PrimitiveTopology::TRIANGLELIST;
	desc.rs = &rasterDesc;
	desc.dss = &depthDesc;
	desc.bs = &blendDesc;
	{ bool ok = device->CreatePipelineState( &desc, &psoAlpha ); GPUP_TRACE("CreatePipelineState psoAlpha ok=%d valid=%d", ok ? 1 : 0, psoAlpha.IsValid() ? 1 : 0); if (!ok || !psoAlpha.IsValid()) GPUP_FAIL("CreatePipelineState psoAlpha"); }

	// additive
	blendDesc.render_target[0].dest_blend = Blend::ONE;
	blendDesc.render_target[0].src_blend_alpha = Blend::ZERO;
	blendDesc.render_target[0].dest_blend_alpha = Blend::ONE;
	{ bool ok = device->CreatePipelineState( &desc, &psoAdd ); GPUP_TRACE("CreatePipelineState psoAdd ok=%d valid=%d", ok ? 1 : 0, psoAdd.IsValid() ? 1 : 0); if (!ok || !psoAdd.IsValid()) GPUP_FAIL("CreatePipelineState psoAdd"); }

	// opaque
	depthDesc.depth_write_mask = DepthWriteMask::ALL;
	blendDesc.render_target[0].blend_enable = false;
	{ bool ok = device->CreatePipelineState( &desc, &psoOpaque ); GPUP_TRACE("CreatePipelineState psoOpaque ok=%d valid=%d", ok ? 1 : 0, psoOpaque.IsValid() ? 1 : 0); if (!ok || !psoOpaque.IsValid()) GPUP_FAIL("CreatePipelineState psoOpaque"); }
	
	// Quad pipeline state (static — same first-draw lifetime requirement as layoutDesc)
	static InputLayout layoutDescQuad;
	layoutDescQuad.elements =
	{
		{ "POSITION", 0, wiGraphics::Format::R32G32_FLOAT, 0, 0, InputClassification::PER_VERTEX_DATA }		
	};
	
	depthDesc.depth_enable = false;
	desc.vs = &shaderQuadVS;
	desc.ps = &shaderQuadDefaultPS;
	desc.il = &layoutDescQuad;
	{ bool ok = device->CreatePipelineState( &desc, &psoQuadDefault ); GPUP_TRACE("CreatePipelineState psoQuadDefault ok=%d valid=%d", ok ? 1 : 0, psoQuadDefault.IsValid() ? 1 : 0); if (!ok || !psoQuadDefault.IsValid()) GPUP_FAIL("CreatePipelineState psoQuadDefault"); }

	desc.ps = &shaderNoisePS;
	{ bool ok = device->CreatePipelineState( &desc, &psoNoise ); GPUP_TRACE("CreatePipelineState psoNoise ok=%d valid=%d", ok ? 1 : 0, psoNoise.IsValid() ? 1 : 0); if (!ok || !psoNoise.IsValid()) GPUP_FAIL("CreatePipelineState psoNoise"); }

	desc.ps = &shaderSpeedPS;
	{ bool ok = device->CreatePipelineState( &desc, &psoSpeed ); GPUP_TRACE("CreatePipelineState psoSpeed ok=%d valid=%d", ok ? 1 : 0, psoSpeed.IsValid() ? 1 : 0); if (!ok || !psoSpeed.IsValid()) GPUP_FAIL("CreatePipelineState psoSpeed"); }

	desc.ps = &shaderPosPS;
	{ bool ok = device->CreatePipelineState( &desc, &psoPos ); GPUP_TRACE("CreatePipelineState psoPos ok=%d valid=%d", ok ? 1 : 0, psoPos.IsValid() ? 1 : 0); if (!ok || !psoPos.IsValid()) GPUP_FAIL("CreatePipelineState psoPos"); }

	// constant buffers
	GPUBufferDesc bd = {};
	bd.usage = Usage::DEFAULT;
	bd.size = sizeof(sPosConstantData);
	bd.bind_flags = BindFlag::CONSTANT_BUFFER;
	//bd.CPUAccessFlags = 0; // removed
	//bd.MiscFlags = 0; // removed
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, nullptr, &posConstants ); GPUP_TRACE("CreateBuffer posConstants ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer posConstants"); }

	bd.usage = Usage::DEFAULT;
	bd.size = sizeof(sSpeedConstantData);
	bd.bind_flags = BindFlag::CONSTANT_BUFFER;
	//bd.CPUAccessFlags = 0; // removed
	//bd.MiscFlags = 0; // removed
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, nullptr, &speedConstants ); GPUP_TRACE("CreateBuffer speedConstants ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer speedConstants"); }

	bd.usage = Usage::DEFAULT;
	bd.size = sizeof(sNoiseConstantData);
	bd.bind_flags = BindFlag::CONSTANT_BUFFER;
	//bd.CPUAccessFlags = 0; // removed
	//bd.MiscFlags = 0; // removed
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, nullptr, &noiseConstants ); GPUP_TRACE("CreateBuffer noiseConstants ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer noiseConstants"); }

	bd.usage = Usage::DEFAULT;
	bd.size = sizeof(sMainVSConstantData);
	bd.bind_flags = BindFlag::CONSTANT_BUFFER;
	//bd.CPUAccessFlags = 0; // removed
	//bd.MiscFlags = 0; // removed
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, nullptr, &mainVSConstants ); GPUP_TRACE("CreateBuffer mainVSConstants ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer mainVSConstants"); }

	bd.usage = Usage::DEFAULT;
	bd.size = sizeof(sMainPSConstantData);
	bd.bind_flags = BindFlag::CONSTANT_BUFFER;
	//bd.CPUAccessFlags = 0; // removed
	//bd.MiscFlags = 0; // removed
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, nullptr, &mainPSConstants ); GPUP_TRACE("CreateBuffer mainPSConstants ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer mainPSConstants"); }

	// 1024 index buffer (obj1)
	mainIndexCountObj1 = 6144;
	SubresourceData data = {};
	data.data_ptr = gpup_1024_indices;
	bd.size = sizeof(unsigned short) * mainIndexCountObj1;
	bd.bind_flags = BindFlag::INDEX_BUFFER;
	//bd.CPUAccessFlags = 0; // removed
	//bd.MiscFlags = 0; // removed
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, data.data_ptr, &mainIndexBufferObj1 ); GPUP_TRACE("CreateBuffer mainIndexBufferObj1 ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer mainIndexBufferObj1"); }

	// 4096 index buffer (obj0)
	mainIndexCountObj0 = 24576;
	data.data_ptr = gpup_4096_indices;
	bd.size = sizeof(unsigned short) * mainIndexCountObj0;
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, data.data_ptr, &mainIndexBufferObj0 ); GPUP_TRACE("CreateBuffer mainIndexBufferObj0 ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer mainIndexBufferObj0"); }

	// 1024 vertex buffer (obj1)
	data.data_ptr = gpup_1024_vertices;
	bd.size = sizeof(GPUP_1024_Vertex) * 4096;
	bd.bind_flags = BindFlag::VERTEX_BUFFER;
	//bd.CPUAccessFlags = 0; // removed
	//bd.MiscFlags = 0; // removed
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, data.data_ptr, &mainVertexBufferObj1 ); GPUP_TRACE("CreateBuffer mainVertexBufferObj1 ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer mainVertexBufferObj1"); }

	// 4096 vertex buffer (obj0)
	data.data_ptr = gpup_4096_vertices;
	bd.size = sizeof(GPUP_4096_Vertex) * 16384;
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, data.data_ptr, &mainVertexBufferObj0 ); GPUP_TRACE("CreateBuffer mainVertexBufferObj0 ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer mainVertexBufferObj0"); }

	// quad vertex buffer
	data.data_ptr = g_VerticesQuad;
	bd.size = sizeof(VertexQuad) * 6;
	bd.bind_flags = BindFlag::VERTEX_BUFFER;
	//bd.CPUAccessFlags = 0; // removed
	//bd.MiscFlags = 0; // removed
	{ bool ok = wiGraphics::GetDevice()->CreateBuffer( &bd, data.data_ptr, &quadVertexBuffer ); GPUP_TRACE("CreateBuffer quadVertexBuffer ok=%d", ok ? 1 : 0); if (!ok) GPUP_FAIL("CreateBuffer quadVertexBuffer"); }

	// samplers
	SamplerDesc samplerDesc = {};
	samplerDesc.address_u = TextureAddressMode::CLAMP;
	samplerDesc.address_v = TextureAddressMode::CLAMP;
	samplerDesc.filter = Filter::MIN_MAG_MIP_POINT;
	device->CreateSampler( &samplerDesc, &samplerPoint );

	samplerDesc.filter = Filter::MIN_MAG_MIP_LINEAR;
	device->CreateSampler( &samplerDesc, &samplerLinear );

	samplerDesc.address_u = TextureAddressMode::WRAP;
	samplerDesc.address_v = TextureAddressMode::WRAP;
	device->CreateSampler( &samplerDesc, &samplerLinearWrap );

	GPUP_TRACE("gpup_init SUCCESS");
	if (gg_trace) { fclose(gg_trace); gg_trace = nullptr; }

	//InitGPUParticlesTest();
	
	return 1;
}

// Load an Effect and set Global Position and Scale. The function returns the effect ID as an integer
int gpup_loadEffect( const char* fl, float x, float y, float z, float s )
{
	return gpup_loadEffectFile( fl, x, y, z, s, 1.0f, 1.0f );
}

// Load an Effect one quality level lower than specified in the effect file and set Global Position, Effect Scale and Particle Scale. This quarters the particle count of the effect. 
// The function returns the effect ID as an integer
int gpup_loadEffectLow( const char* fl, float x, float y, float z, float s, float ps )
{
	return gpup_loadEffectFile( fl, x, y, z, s, 0.5f, ps );
}

// Load an Effect one quality level higher than specified in the effect file and set Global Position, Effect Scale and Particle Scale. This quadruples the particle count of the effect. 
// The function returns the effect ID as an integer
int gpup_loadEffectHigh( const char* fl, float x, float y, float z, float s, float ps )
{
	return gpup_loadEffectFile( fl, x, y, z, s, 2.0f, ps );
}

// Activate or deactivate the emitter of a specific effect. 
void gpup_emitterActive( int ID, int active )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;
	int iOldActiveState = gpup_emitter[ID].emitterActive;
	gpup_emitter[ID].emitterActive = active ? 1 : 0;

	//LB: each time an emitter is made active, do a burst fire (good for non looping one time particles)
	if (iOldActiveState == 0 && gpup_emitter[ID].emitterActive != 0 )
	{
		gpup_emitterFire(ID);
	}
}

// Set a new global position for the effect. Can be used to move an effect including all emitted particles and the turbulence
void gpup_setGlobalPosition( int ID, float x, float y, float z )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].globalx[0] = x;
	gpup_emitter[ID].globaly[0] = y;
	gpup_emitter[ID].globalz[0] = z;
	if ( gpup_emitter[ID].effectLoaded == 1 )
	{
		gpup_emitter[ID].mainVSConstantData.globalpos[0].x = gpup_emitter[ID].globalx[0];
		gpup_emitter[ID].mainVSConstantData.globalpos[0].y = gpup_emitter[ID].globaly[0];
		gpup_emitter[ID].mainVSConstantData.globalpos[0].z = gpup_emitter[ID].globalz[0];
		gpup_emitter[ID].mainVSConstantData.globalpos[0].w = (float) gpup_getSubPositionCount(ID) ;
	}
}

// Added this so I can influence emitter direction!
void gpup_getEmitterSpeedAngleAdjustment(int ID, float* x, float* y, float* z)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;
	*x = gpup_emitter[ID].emitter_speed_x;
	*y = gpup_emitter[ID].emitter_speed_y;
	*z = gpup_emitter[ID].emitter_speed_z;
}

void gpup_setEmitterSpeedAngleAdjustment(int ID, float x, float y, float z)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;
	if (gpup_emitter[ID].emitter_type == 0 || gpup_emitter[ID].emitter_type == 2)
	{
		// only affects:
		// 0 = box spawn emitters (newly added for fountain directional)
		// 2 = smoke style emitters
		gpup_emitter[ID].emitter_speedadjustment_x = x;
		gpup_emitter[ID].emitter_speedadjustment_y = y;
		gpup_emitter[ID].emitter_speedadjustment_z = z;
		if (gpup_emitter[ID].effectLoaded == 1)
		{
			float fFinalSpeedX = gpup_emitter[ID].emitter_speedadjustment_x;
			float fFinalSpeedY = gpup_emitter[ID].emitter_speedadjustment_y;
			float fFinalSpeedZ = gpup_emitter[ID].emitter_speedadjustment_z;
			gpup_emitter[ID].speedConstantData.speedvar.x = (fFinalSpeedX - 0.5f) * 0.1f;
			gpup_emitter[ID].speedConstantData.speedvar.y = (fFinalSpeedY - 0.5f) * 0.1f;
			gpup_emitter[ID].speedConstantData.speedvar.z = (fFinalSpeedZ - 0.5f) * 0.1f;
		}
	}
}

// Set a new Global Porition for the effect without moving already existing particles. The vector and noise fields will be moved to the new position.
void gpup_setGlobalPivot( int ID, float x, float y, float z )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].newX = x;
	gpup_emitter[ID].newY = y;
	gpup_emitter[ID].newZ = z;
	gpup_emitter[ID].pivotMoved = 1 ;
}

// Change the global scale of an effect
void gpup_setGlobalScale( int ID, float s )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].globalSize = s;
	if ( gpup_emitter[ID].effectLoaded == 1 )
	{
		gpup_emitter[ID].mainVSConstantData.globalsize.x = gpup_emitter[ID].globalSize;
		gpup_emitter[ID].mainVSConstantData.globalsize.y = gpup_emitter[ID].globalSize;
		gpup_emitter[ID].mainVSConstantData.globalsize.z = gpup_emitter[ID].globalSize;
	}
}

// Change the size of each particle. This will only scale each particle sprite and not have an effect on the overall size and flow of the effect.
void gpup_setParticleScale( int ID, float s )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].particleSize = s;
	if ( gpup_emitter[ID].effectLoaded == 1 ) 
	{
		gpup_emitter[ID].mainVSConstantData.pgrow.x = 0.001f+gpup_emitter[ID].size1*gpup_emitter[ID].sizer;
		gpup_emitter[ID].mainVSConstantData.pgrow.y = 0.001f+gpup_emitter[ID].size2*gpup_emitter[ID].sizer;
		gpup_emitter[ID].mainVSConstantData.pgrow.z = gpup_emitter[ID].particleSize;
	}
}

// Change the global rotation of an effect
#define MR_PI    ( ( float ) 3.141592654f )
void gpup_setGlobalRotation( int ID, float x, float y, float z )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;	
	gpup_emitter[ID].globalRotX = (x) * (MR_PI / 180.0f);
	gpup_emitter[ID].globalRotY = (y) * (MR_PI / 180.0f);
	gpup_emitter[ID].globalRotZ = (z) * (MR_PI / 180.0f);
	if ( gpup_emitter[ID].effectLoaded == 1 )
	{
		if (gpup_emitter[ID].emitter_type != 2)
		{
			// rings should be rotatable
			gpup_emitter[ID].mainVSConstantData.globalrot.x = gpup_emitter[ID].globalRotX;
			gpup_emitter[ID].mainVSConstantData.globalrot.y = gpup_emitter[ID].globalRotY;
			gpup_emitter[ID].mainVSConstantData.globalrot.z = gpup_emitter[ID].globalRotZ;
		}
		else
		{
			// for smoke projecting outward
			gpup_emitter[ID].mainVSConstantData.globalrot.x = 0;
			gpup_emitter[ID].mainVSConstantData.globalrot.y = 0;
			gpup_emitter[ID].mainVSConstantData.globalrot.z = 0;
		}
	}
}

// Set the local position of an emitter
void gpup_setLocalPosition( int ID, float x, float y, float z )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	// coordinates already passed in as relative to the particle global position
	gpup_emitter[ID].emitter_local_x = (x) / (gpup_emitter[ID].varea * gpup_emitter[ID].globalSize * 2.0f) + 0.5f;
	gpup_emitter[ID].emitter_local_y = (y) / (gpup_emitter[ID].varea * gpup_emitter[ID].globalSize * 2.0f) + 0.5f;
	gpup_emitter[ID].emitter_local_z = (z) / (gpup_emitter[ID].varea * gpup_emitter[ID].globalSize * 2.0f) + 0.5f;
	if ( gpup_emitter[ID].effectLoaded == 1 )
	{
		gpup_emitter[ID].posConstantData.localemitter.x = gpup_emitter[ID].emitter_local_x * 4.0f - 2.0f;
		gpup_emitter[ID].posConstantData.localemitter.y = gpup_emitter[ID].emitter_local_y * 4.0f - 2.0f;
		gpup_emitter[ID].posConstantData.localemitter.z = gpup_emitter[ID].emitter_local_z * 4.0f - 2.0f;
	}
}
void gpup_resetLocalPosition(int ID)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;

	// ignores local offset passed in so emitter spawns from exact spot of particle game element (if called)
	gpup_emitter[ID].emitter_local_x = 0.5f;
	gpup_emitter[ID].emitter_local_y = 0.5f;
	gpup_emitter[ID].emitter_local_z = 0.5f;
	if (gpup_emitter[ID].effectLoaded == 1)
	{
		gpup_emitter[ID].posConstantData.localemitter.x = gpup_emitter[ID].emitter_local_x * 4.0f - 2.0f;
		gpup_emitter[ID].posConstantData.localemitter.y = gpup_emitter[ID].emitter_local_y * 4.0f - 2.0f;
		gpup_emitter[ID].posConstantData.localemitter.z = gpup_emitter[ID].emitter_local_z * 4.0f - 2.0f;
	}
}

// Set a Subemitter's position. Subemitter 0 is the overall global position of the effect.
void gpup_setSubPosition( int ID, int pos, float x, float y, float z )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].globalx[pos] = x;
	gpup_emitter[ID].globaly[pos] = y;
	gpup_emitter[ID].globalz[pos] = z;
	if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
}

// Add a sub-emitter
int gpup_addSubPosition( int ID, float x, float y, float z )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return 0;

	int se = 0;
	int i = 0;
		
	for( int i = 1; i < 8; i++ )
	{
		if ( gpup_emitter[ID].subEmitterUsed[i] == 0 )
		{
			se = i;
			break;
		}
	}
	if ( se == 0 ) return 0;

	gpup_emitter[ID].globalx[se] = x; //  /(gpup_emitter[ID].varea*gpup_emitter[ID].globalSize*2.0f)+0.5f
	gpup_emitter[ID].globaly[se] = y; //  /(gpup_emitter[ID].varea*gpup_emitter[ID].globalSize*2.0f)+0.5f
	gpup_emitter[ID].globalz[se] = z; //  /(gpup_emitter[ID].varea*gpup_emitter[ID].globalSize*2.0f)+0.5f
	gpup_emitter[ID].subEmitterUsed[se] = 1;
	gpup_emitter[ID].subEmitters = gpup_emitter[ID].subEmitters + 1;
	if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
	
	return se;
}

// Delete a sub-emitter
int gpup_deleteSubPosition( int ID, int pos )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return 0;

	int se = -1;
	if ( pos > 0 && pos < 8 && gpup_emitter[ID].subEmitterUsed[pos] == 1 )
	{
		gpup_emitter[ID].subEmitterUsed[pos] = 0;
		gpup_emitter[ID].subEmitters = gpup_emitter[ID].subEmitters - 1;
		se = 1;
		if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
	}
	
	return se;
}

// query the number of current sub-emitters
int gpup_getSubPositionCount( int ID )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return 0;

	int se = 0;
	for( int i = 0; i < 8; i++ )
	{
		if ( gpup_emitter[ID].subEmitterUsed[i] == 1 ) se++;
	}
	
	return se;
}

// Fire a burst emitter
void gpup_emitterBurstLoopAutoMode(int ID, int iAuto)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;
	gpup_emitter[ID].emitter_burst_auto = iAuto;
}
void gpup_emitterBurstMode(int ID, int iMode)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;
	gpup_emitter[ID].emitter_burst_mode = iMode;
}
void gpup_emitterFire(int ID)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;
	gpup_emitter[ID].emitter_burst_fire = 1;
}

// Set the Blending Mode for an Effect. 
void gpup_setBlendmode( int ID, int usel )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;
	
	gpup_emitter[ID].blendmode = usel; // 0 = opaque, 1 = opaque, 2 = opaque, 3 = alpha, 4 = additive
		
	if ( gpup_emitter[ID].effectLoaded == 1 )
	{
		if ( usel == 0 ) usel = 1;
		gpup_emitter[ID].mainPSConstantData.moder = (float) usel;
	}
}

// Toggle the auto fire option for burst emitters
void gpup_setAutoFire( int ID, int act )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;
	
	gpup_emitter[ID].emitter_burst_auto = act;
}

// Sets the parameters of the floor reflection
void gpup_floorReflection( int ID, int active, float height ) 
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;
	
	gpup_emitter[ID].floorHeight = (height - gpup_emitter[ID].globaly[0]) / (gpup_emitter[ID].varea * gpup_emitter[ID].globalSize * 2.0f) + 0.5f;
	gpup_emitter[ID].floorActive = (active == 1) ? 1.0f : 0.0f;
	
	if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
}
void gpup_restoreFloorReflection(int ID, int active, float height)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;
	gpup_emitter[ID].floorHeight = height;
	gpup_emitter[ID].floorActive = (active == 1) ? 1.0f : 0.0f;
	if (gpup_emitter[ID].effectLoaded == 1) gpup_updatesettings(ID);
}

// Sets the parameters of the spherical reflection
void gpup_sphereReflection( int ID, int active, float x, float y, float z, float radius ) 
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].sphereX = (x - gpup_emitter[ID].globalx[0]) / (gpup_emitter[ID].varea * gpup_emitter[ID].globalSize * 2.0f) + 0.5f;
	gpup_emitter[ID].sphereY = (y - gpup_emitter[ID].globaly[0]) / (gpup_emitter[ID].varea * gpup_emitter[ID].globalSize * 2.0f) + 0.5f;
	gpup_emitter[ID].sphereZ = (z - gpup_emitter[ID].globalz[0]) / (gpup_emitter[ID].varea * gpup_emitter[ID].globalSize * 2.0f) + 0.5f;
	gpup_emitter[ID].sphereRadius = radius / (gpup_emitter[ID].varea * gpup_emitter[ID].globalSize * 0.5f);
		
	gpup_emitter[ID].sphereActive = (active == 1) ? 1.0f : 0.0f;
	
	if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
}

// Change the bounciness of the particles of an effect with reflections
void gpup_setBounciness( int ID, float value )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;
	
	if ( value < 0.0f ) value = 0.0f;
	if ( value > 1.0f ) value = 1.0f;
	gpup_emitter[ID].bounciness = value;
	if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
}


// Sets the end points of line and beam emitters
void gpup_setLine( int ID, float x1, float y1, float z1, float x2, float y2, float z2 )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].globalx[0] = (x1 + x2) * 0.5f;
	gpup_emitter[ID].globaly[0] = (y1 + y2) * 0.5f;
	gpup_emitter[ID].globalz[0] = (z1 + z2) * 0.5f;
	gpup_emitter[ID].emitter_x1 = x1 - gpup_emitter[ID].globalx[0];
	gpup_emitter[ID].emitter_y1 = y1 - gpup_emitter[ID].globaly[0];
	gpup_emitter[ID].emitter_z1 = z1 - gpup_emitter[ID].globalz[0];
	gpup_emitter[ID].emitter_x2 = x2 - gpup_emitter[ID].globalx[0];
	gpup_emitter[ID].emitter_y2 = y2 - gpup_emitter[ID].globaly[0];
	gpup_emitter[ID].emitter_z2 = z2 - gpup_emitter[ID].globalz[0];
	if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
}

// Sets the end points of line and beam emitters
void gpup_setLineLocal( int ID, float x1, float y1, float z1, float x2, float y2, float z2 )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].emitter_x1 = x1 - gpup_emitter[ID].globalx[0];
	gpup_emitter[ID].emitter_y1 = y1 - gpup_emitter[ID].globaly[0];
	gpup_emitter[ID].emitter_z1 = z1 - gpup_emitter[ID].globalz[0];
	gpup_emitter[ID].emitter_x2 = x2 - gpup_emitter[ID].globalx[0];
	gpup_emitter[ID].emitter_y2 = y2 - gpup_emitter[ID].globaly[0];
	gpup_emitter[ID].emitter_z2 = z2 - gpup_emitter[ID].globalz[0];
	if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
}

// Adds more than one beam to a line emitter (up to 10) and sets the offset between the duplicated beams. These beams share
// the particles of the effect, so set the emit rate high enough to supportthe desired amount of beams. 
// The function also has a parameter to make the beam curl around it's main axis. 
void gpup_setLineBeams( int ID, int beamcount, float beamoffset, float beamcurl )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	if ( beamcount < 1 ) beamcount = 1;
	if ( beamcount > 10 ) beamcount = 10;
	gpup_emitter[ID].emitter_linebeams = (float) beamcount;
	gpup_emitter[ID].emitter_linebeamoffset = beamoffset;
	gpup_emitter[ID].emitter_lineswirl = beamcurl;
	if ( gpup_emitter[ID].effectLoaded == 1 ) gpup_updatesettings( ID );
}

// Set a global Tint for an effect. This helps to integrate alpha and clip blended effects into your scene. 
void gpup_setEffectColor( int ID, int r, int g, int b )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;
	
	gpup_emitter[ID].r = 0.00390625f * r;
	gpup_emitter[ID].g = 0.00390625f * g;
	gpup_emitter[ID].b = 0.00390625f * b;
	if ( gpup_emitter[ID].effectLoaded == 1 ) 
	{
		gpup_emitter[ID].mainPSConstantData.clr.x = gpup_emitter[ID].r;
		gpup_emitter[ID].mainPSConstantData.clr.y = gpup_emitter[ID].g;
		gpup_emitter[ID].mainPSConstantData.clr.z = gpup_emitter[ID].b;
	}
}

void gpup_setEffectOpacity(int ID, float a)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;

	gpup_emitter[ID].opacity = a;
	if (gpup_emitter[ID].effectLoaded == 1)
	{
		gpup_emitter[ID].mainPSConstantData.opacity = gpup_emitter[ID].opacity;
	}
}

// Activate or deactivate smooth blending of spritesheet animations
void gpup_setSmoothBlending( int ID, int act )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;
	
	if ( gpup_emitter[ID].effectLoaded == 1  )
	{
		gpup_emitter[ID].smooth_blending = act;
		
		gpup_emitter[ID].mainPSConstantData.tile.x = (float) gpup_emitter[ID].ani_x;
		gpup_emitter[ID].mainPSConstantData.tile.y = (float) gpup_emitter[ID].ani_y;
		gpup_emitter[ID].mainPSConstantData.tile.z = (float) gpup_emitter[ID].ani_s;
		gpup_emitter[ID].mainPSConstantData.tile.w = (float) act;
		
		gpup_emitter[ID].mainVSConstantData.tilex.x = (float) gpup_emitter[ID].ani_x;
		gpup_emitter[ID].mainVSConstantData.tilex.y = (float) gpup_emitter[ID].ani_y;
		gpup_emitter[ID].mainVSConstantData.tilex.z = (float) gpup_emitter[ID].ani_s;
		gpup_emitter[ID].mainVSConstantData.tilex.w = (float) act;
	}
}

void gpup_deleteAllEffects(void)
{
	for (int i = 0; i < gpup_maxeffects; i++)
	{
		gpup_deleteEffect(i);
	}
}

// Delete a loaded effect. 
int gpup_deleteEffect( int ID )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return -1;
	if ( gpup_emitter[ID].effectLoaded == 0 ) return -1;
	
	GPUP_DeleteTexture( &gpup_emitter[ID].t_field );
	GPUP_DeleteTexture( &gpup_emitter[ID].texNoise );
	GPUP_DeleteTexture( &gpup_emitter[ID].texPos[0] );
	GPUP_DeleteTexture( &gpup_emitter[ID].texPos[1] );
	GPUP_DeleteTexture( &gpup_emitter[ID].texSpeed[0] );
	GPUP_DeleteTexture( &gpup_emitter[ID].texSpeed[1] );
	GPUP_DeleteTexture( &gpup_emitter[ID].image1 );
	GPUP_DeleteTexture( &gpup_emitter[ID].imagex );
	GPUP_DeleteTexture( &gpup_emitter[ID].gradient_1 );

	// GGMAX 3.65 VRAM LEAK (second half): clearing the Texture MEMBERS above is not enough.
	// RenderPassAttachment stores a Texture BY VALUE (wiGraphics.h) and CreateRenderPass copies the
	// whole desc, so each of these five render passes holds its OWN live reference to texPos /
	// texSpeed / texNoise. Release the members and the passes still pin the memory - which is exactly
	// what the census showed after the first half of this fix landed: "imageTex" went to zero retained
	// (no render pass references those) while "renderTex" stayed at +7.5 MB.
	gpup_emitter[ID].renderPassPos[0]   = RenderPass();
	gpup_emitter[ID].renderPassPos[1]   = RenderPass();
	gpup_emitter[ID].renderPassSpeed[0] = RenderPass();
	gpup_emitter[ID].renderPassSpeed[1] = RenderPass();
	gpup_emitter[ID].renderPassNoise    = RenderPass();

	gpup_emitter[ID].effectLoaded = 0 ;
			
	return 1;
}

// Toggle for disabling the rendering of an effect without stopping the simulation
void gpup_setEffectVisible( int ID, int visible )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;

	gpup_emitter[ID].effectVisible = visible;
}

int gpup_getEffectVisible ( int ID )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return 0;
	return gpup_emitter[ID].effectVisible;
}
float gpup_getParticleSpeed(int ID)
{
	if (ID < 0 || ID >= gpup_maxeffects) return 0;
	return gpup_emitter[ID].emitter_animation_speed;
}
float gpup_getParticleOpacity(int ID)
{
	if (ID < 0 || ID >= gpup_maxeffects) return 0;
	return gpup_emitter[ID].opacity;
}
float gpup_getParticleSize(int ID)
{
	if (ID < 0 || ID >= gpup_maxeffects) return 0;
	return gpup_emitter[ID].particleSize;
}
float gpup_getBounciness(int ID)
{
	if (ID < 0 || ID >= gpup_maxeffects) return 0;
	return gpup_emitter[ID].bounciness;
}
float gpup_getFloorReflectionHeight(int ID)
{
	if (ID < 0 || ID >= gpup_maxeffects) return 0;
	return gpup_emitter[ID].floorHeight;
}
void gpup_getEffectColor(int ID, float* r, float* g, float* b)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;
	*r = gpup_emitter[ID].r / 0.00390625f;
	*g = gpup_emitter[ID].g / 0.00390625f;
	*b = gpup_emitter[ID].b / 0.00390625f;
}
float gpup_getEffectLifespan(int ID)
{
	if (ID < 0 || ID >= gpup_maxeffects) return 0;
	return (gpup_emitter[ID].lifespan * 60 * 10);
}

void gpup_setEffectLifespan(int ID, float value)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;
	
	gpup_emitter[ID].lifespan = value / (60 * 10);
}

void gpup_setEffectAnimationSpeed(int ID,float speed)
{
	if (ID < 0 || ID >= gpup_maxeffects) return;

	gpup_emitter[ID].emitter_animation_speed = speed;
}

// Toggle bilinear filtering on the particle sprites
void gpup_setBilinear( int ID, int active )
{
	if ( ID < 0 || ID >= gpup_maxeffects ) return;
}

float g_fSlowParticleTime = 1.0f;


// Update the Particles
// GGMAX 2026-08-07 (tasks #120/#121): harness instruments. The emitter/settings structs are
// private to this translation unit, so the dump lives here and the harness calls in blind.
void gpup_debug_show( int on )        { gg_gpup_draw_enable = (on != 0); }
void gpup_debug_force_arm( int mode ) { gg_gpup_force_arm = mode; }
// Fast-forward the sn clock (GPUP_SET_SN) — the hash-degradation bug needed ~50 min of
// runtime to show; with this the broken magnitude is reachable in one command. With the
// fmod fix the look must be INVARIANT to this value.
void gpup_debug_set_sn( float v )     { gpup_settings.sn = v; }
// GPUP_SET_CLOCKS <sn> <rotsn> <agk_seconds> — full clock injection for bisecting the
// ~55-min white-out: sn was exonerated (invariance at 500k), leaving rotsn and agk_time
// (twins at ~1/s). Rebasing the QPC start makes AGKTimer() return agk_seconds from now on.
void gpup_debug_set_clocks( float sn, float rotsn, float agk_seconds )
{
	gpup_settings.sn = sn;
	gpup_settings.rotsn = rotsn;
	int64_t i64Now = 0;
	QueryPerformanceCounter( (LARGE_INTEGER*)&i64Now );
	i64StartTime = i64Now - (int64_t)( (double)agk_seconds * (double)i64TimeFreq );
}
// GGMAX 2026-08-08 (task #120 white-out): GPU-side evidence. Every CPU-visible input to the
// sim is exonerated (constants byte-identical healthy-vs-broken, clocks cleared by injection
// in BOTH directions, rings/pools flat) — what remains is texture CONTENT. texNoiseOrig and
// texDist2 are created ONCE per process (gpup_init) and consumed by every emitter every sim
// tick; they are the only gpup GPU state that survives a level reload, matching the defect's
// process-lifetime signature. CRC of noiseOrig/dist2 is load-invariant (sourced from PNGs),
// so healthy-vs-broken comparison needs no baseline from the same run.
static void GG_DumpTextureStats( FILE* f, const char* label, const Texture& tex )
{
	if ( !tex.IsValid() ) { fprintf( f, "  tex %-10s: INVALID\n", label ); return; }
	wi::vector<uint8_t> data;
	if ( !wi::helper::saveTextureToMemory( tex, data ) || data.empty() )
	{
		fprintf( f, "  tex %-10s: READBACK FAILED\n", label );
		return;
	}
	uint32_t crc = 0xFFFFFFFFu;
	uint64_t sum[4] = { 0, 0, 0, 0 };
	uint64_t sat0 = 0, sat255 = 0;
	for ( size_t i = 0; i < data.size(); i++ )
	{
		uint8_t b = data[i];
		crc ^= b;
		for ( int k = 0; k < 8; k++ ) crc = ( crc >> 1 ) ^ ( 0xEDB88320u & ( 0u - ( crc & 1u ) ) );
		sum[i & 3] += b;
		if ( b == 0 ) sat0++; else if ( b == 255 ) sat255++;
	}
	size_t px = data.size() / 4;
	fprintf( f, "  tex %-10s: %ux%u crc=%08X mean=(%.1f,%.1f,%.1f,%.1f) sat0=%.1f%% sat255=%.1f%%\n",
		label, tex.desc.width, tex.desc.height, crc ^ 0xFFFFFFFFu,
		px ? (double)sum[0] / px : 0.0, px ? (double)sum[1] / px : 0.0,
		px ? (double)sum[2] / px : 0.0, px ? (double)sum[3] / px : 0.0,
		100.0 * sat0 / (double)data.size(), 100.0 * sat255 / (double)data.size() );
}

// GGMAX 2026-08-08 (task #120 round 2): the motion-arm catch proved the SIM POOLS ARE
// HEALTHY inside a live white-out (pos/speed readback stats == healthy baseline, input
// texture CRCs unchanged) while GPUP_SHOW pinned the white on gpup quads — the defect is
// DRAW-side. By elimination the only process-lifetime draw inputs left are the five static
// buffers below, uploaded once at init from compile-time arrays. Ground truth lives in the
// exe, so the GPU copy is verified BYTE-FOR-BYTE and any scribble is fingerprinted (hex
// window at first diff — the pattern names the scribbler: float positions = a mesh upload,
// zeros = a clear, etc). No barriers: D3D12 buffer state DECAYS to COMMON between submits
// and the copy promotes COMMON->COPY_SOURCE implicitly.
static void GG_CheckStaticBuffer( FILE* f, const char* label, const GPUBuffer& buf,
	const void* src, size_t srcSize )
{
	if ( !buf.IsValid() ) { fprintf( f, "  buf %-10s: INVALID\n", label ); return; }
	GraphicsDevice* device = wiGraphics::GetDevice();
	GPUBuffer staging;
	GPUBufferDesc sd = buf.GetDesc();
	sd.usage = Usage::READBACK;
	sd.bind_flags = BindFlag::NONE;
	sd.misc_flags = ResourceMiscFlag::NONE;
	if ( !device->CreateBuffer( &sd, nullptr, &staging ) )
	{
		fprintf( f, "  buf %-10s: STAGING FAILED\n", label );
		return;
	}
	CommandList cmd = device->BeginCommandList();
	device->CopyResource( &staging, &buf, cmd );
	device->SubmitCommandLists();
	device->WaitForGPU();
	if ( staging.mapped_data == nullptr )
	{
		fprintf( f, "  buf %-10s: NO MAPPED DATA\n", label );
		return;
	}
	const uint8_t* gpu = (const uint8_t*)staging.mapped_data;
	const uint8_t* ref = (const uint8_t*)src;
	size_t diffs = 0, first = (size_t)-1, last = 0;
	for ( size_t i = 0; i < srcSize; i++ )
	{
		if ( gpu[i] != ref[i] )
		{
			if ( first == (size_t)-1 ) first = i;
			last = i;
			diffs++;
		}
	}
	if ( diffs == 0 )
	{
		fprintf( f, "  buf %-10s: %zu bytes MATCH source\n", label, srcSize );
		return;
	}
	fprintf( f, "  buf %-10s: *** %zu/%zu bytes DIFFER first=0x%zX last=0x%zX\n",
		label, diffs, srcSize, first, last );
	size_t w0 = first > 8 ? first - 8 : 0;
	fprintf( f, "    got:" );
	for ( size_t i = w0; i < w0 + 32 && i < srcSize; i++ ) fprintf( f, " %02X", gpu[i] );
	fprintf( f, "\n    ref:" );
	for ( size_t i = w0; i < w0 + 32 && i < srcSize; i++ ) fprintf( f, " %02X", ref[i] );
	fprintf( f, "\n" );
}

// GGMAX 2026-08-08 (task #122 round 4, USER-REQUESTED): per-particle journey telemetry.
// Reads back texPos/texSpeed[currImage] and decodes a fixed set of particle slots exactly
// as MainVS does (fl4unpack quadrants: x at (i,j), y at (i+P,j), z at (i,j+P), age at
// (i+P,j+P)), appending one CSV row per slot to Files/gpup_track.csv. Sampled at ~1Hz over
// a minute this gives every tracked particle's age/position/velocity trajectory from
// emission to death — a mass mid-life position jump, a mass age reset, or an age stall
// is directly visible in the data instead of inferred from screenshots.
static double GG_Unpack4( const uint8_t* b )
{
	// matches shader fl4unpack: v = byte/255 exactly; dot with (1, 1/255, 1/65025, 1/16581375)
	return b[0] / 255.0 + b[1] / 65025.0 + b[2] / 16581375.0 + b[3] / 4228250625.0;
}

int gpup_debug_track( int enr )
{
	if ( enr < 0 || enr >= gpup_maxeffects ) return -1;
	if ( gpup_emitter[enr].effectLoaded == 0 ) return -1;
	const int P = gpup_emitter[enr].particles;
	const int dim = P * 2;
	const int curr = gpup_emitter[enr].currImage;
	wi::vector<uint8_t> pos, spd;
	if ( !wi::helper::saveTextureToMemory( gpup_emitter[enr].texPos[curr], pos ) ) return -2;
	if ( !wi::helper::saveTextureToMemory( gpup_emitter[enr].texSpeed[curr], spd ) ) return -2;
	if ( (int)pos.size() < dim * dim * 4 || (int)spd.size() < dim * dim * 4 ) return -3;

	FILE* f = nullptr;
	f = GGDiagFopen("gpup_track.csv", "a");   // game CWD is Max/Files already
	if ( !f ) return -4;
	const float t = AGKTimer();
	// one settings row per call: every clock/warp the event could correlate with
	fprintf( f, "S,%.3f,%d,%d,%.2f,%.4f,%.4f,%.4f,%.2f,%d\n",
		t, enr, curr, gpup_settings.sn, gpup_settings.rotsn, gpup_settings.time,
		gpup_emitter[enr].noiseZaehler, gpup_emitter[enr].spawnint, gpup_emitter[enr].testpos );
	// 120 fixed pseudo-scattered slots — same slots every call so rows chain into trajectories
	for ( int si = 0; si < 120; si++ )
	{
		const int i = (si * 7) % P;
		const int j = (si * 11) % P;
		const uint8_t* px = &pos[((size_t)j * dim + i) * 4];
		const uint8_t* py = &pos[((size_t)j * dim + i + P) * 4];
		const uint8_t* pz = &pos[((size_t)(j + P) * dim + i) * 4];
		const uint8_t* pa = &pos[((size_t)(j + P) * dim + i + P) * 4];
		const uint8_t* sx = &spd[((size_t)j * dim + i) * 4];
		const uint8_t* sy = &spd[((size_t)j * dim + i + P) * 4];
		const uint8_t* sz = &spd[((size_t)(j + P) * dim + i) * 4];
		fprintf( f, "P,%.3f,%d,%d,%.7f,%.7f,%.7f,%.7f,%.7f,%.7f,%.7f\n",
			t, enr, si,
			GG_Unpack4( pa ),                 // age [0,1]
			GG_Unpack4( px ) - 0.5, GG_Unpack4( py ) - 0.5, GG_Unpack4( pz ) - 0.5,   // pos, emitter units
			GG_Unpack4( sx ) - 0.5, GG_Unpack4( sy ) - 0.5, GG_Unpack4( sz ) - 0.5 ); // speed
	}
	fclose( f );
	return 120;
}

// GGMAX 2026-08-08 (task #122 round 7): FULL age-plane snapshot — decode ALL P*P slot ages
// into one CSV row (t, enr, testpos, a0..a(P*P-1) as 0-255 ints, slot index = j*P+i, matching
// the shader's fr). Polled across a mass-respawn event, the per-event BIRTH MASK exposes the
// exact geometry of the over-spawn (comb spacing / column stripes / quadrant blocks), which
// the 120-slot sampler cannot resolve.
int gpup_debug_ages( int enr )
{
	if ( enr < 0 || enr >= gpup_maxeffects ) return -1;
	if ( gpup_emitter[enr].effectLoaded == 0 ) return -1;
	const int P = gpup_emitter[enr].particles;
	const int dim = P * 2;
	const int curr = gpup_emitter[enr].currImage;
	wi::vector<uint8_t> pos;
	if ( !wi::helper::saveTextureToMemory( gpup_emitter[enr].texPos[curr], pos ) ) return -2;
	if ( (int)pos.size() < dim * dim * 4 ) return -3;
	FILE* f = nullptr;
	f = GGDiagFopen("gpup_ages.csv", "a");
	if ( !f ) return -4;
	fprintf( f, "A,%.3f,%d,%d", AGKTimer(), enr, gpup_emitter[enr].testpos );
	for ( int j = 0; j < P; j++ )
	{
		for ( int i = 0; i < P; i++ )
		{
			const uint8_t* pa = &pos[((size_t)(j + P) * dim + i + P) * 4];
			fprintf( f, ",%d", (int)(GG_Unpack4( pa ) * 255.0 + 0.5) );
		}
	}
	fprintf( f, "\n" );
	fclose( f );
	return P * P;
}

// GGMAX 2026-08-08 (task #120): live confirm/refute lever — re-create every process-lifetime
// gpup GPU resource from its in-exe/PNG source on a BROKEN instance. If the white-out clears,
// static-resource poisoning is CONFIRMED (and GPUP_DUMP's byte-compare says which one); the
// durable fix is then re-creating them at level load. Old objects are replaced by assignment;
// the engine's deferred-destroy keeps the old GPU resources alive for in-flight frames, so
// this is safe from the harness thread.
void gpup_debug_regen_textures()
{
	if ( !gpu_particles_initialised ) return;
	GPUP_LoadTexture( "Files/effectbank/common/noise64.png", &texNoiseOrig );
	GPUP_LoadTexture( "Files/effectbank/common/dist2.png", &texDist2 );

	GraphicsDevice* device = wiGraphics::GetDevice();
	GPUBufferDesc bd = {};
	bd.usage = Usage::DEFAULT;
	bd.bind_flags = BindFlag::INDEX_BUFFER;
	bd.size = sizeof(unsigned short) * 6144;
	device->CreateBuffer( &bd, gpup_1024_indices, &mainIndexBufferObj1 );
	bd.size = sizeof(unsigned short) * 24576;
	device->CreateBuffer( &bd, gpup_4096_indices, &mainIndexBufferObj0 );
	bd.bind_flags = BindFlag::VERTEX_BUFFER;
	bd.size = sizeof(GPUP_1024_Vertex) * 4096;
	device->CreateBuffer( &bd, gpup_1024_vertices, &mainVertexBufferObj1 );
	bd.size = sizeof(GPUP_4096_Vertex) * 16384;
	device->CreateBuffer( &bd, gpup_4096_vertices, &mainVertexBufferObj0 );
	bd.size = sizeof(VertexQuad) * 6;
	device->CreateBuffer( &bd, g_VerticesQuad, &quadVertexBuffer );

	// round-3 (task #120): SAMPLERS — the one gpup GPU object class a resource readback
	// can never validate. The draw reaches every texture THROUGH a sampler descriptor; if
	// samplerPoint's slot got stomped to LINEAR, the VS's fl4unpack blends packed pool
	// bytes into garbage positions/ages from a perfectly healthy pool — white screen with
	// every content check green. Re-creating them allocates fresh descriptor slots; if a
	// live white-out clears at this REGEN, the sampler/descriptor stomp is CONFIRMED.
	SamplerDesc samplerDesc = {};
	samplerDesc.address_u = TextureAddressMode::CLAMP;
	samplerDesc.address_v = TextureAddressMode::CLAMP;
	samplerDesc.filter = Filter::MIN_MAG_MIP_POINT;
	device->CreateSampler( &samplerDesc, &samplerPoint );
	samplerDesc.filter = Filter::MIN_MAG_MIP_LINEAR;
	device->CreateSampler( &samplerDesc, &samplerLinear );
	samplerDesc.address_u = TextureAddressMode::WRAP;
	samplerDesc.address_v = TextureAddressMode::WRAP;
	device->CreateSampler( &samplerDesc, &samplerLinearWrap );
}

// GGMAX 2026-08-08 (task #120): shader canary lever — see GPUP_MainVS.hlsl. mode 0 = off,
// 1 = red dots at pool positions (bypasses size/alpha/color/billboard math), 2 = red dots
// at grid positions (bypasses the pool samples too). Rides the spare padding1/filler
// constants, so no struct/layout change. New effect loads reset their structs = canary off.
// Task #122 round 6 — appearance-path bisect modes (full render minus ONE ingredient):
// 3 = no sprite rotation (ang=0), 4 = no gradient influence (flat color/alpha),
// 5 = fixed size gradient, 6 = no distortion offset (PS), 7 = base image quadrant only (PS).
void gpup_debug_canary( int mode )
{
	for ( int i = 0; i < gpup_maxeffects; i++ )
	{
		gpup_emitter[i].mainVSConstantData.padding1 = (mode <= 5 || mode == 8) ? (float)mode : 0.0f;
		gpup_emitter[i].mainPSConstantData.filler = (mode == 1 || mode == 2) ? 1.0f : (mode == 8 ? 3.0f : 0.0f);
		gpup_emitter[i].mainPSConstantData.filler2 = (mode == 6) ? 1.0f : (mode == 7 ? 2.0f : 0.0f);
	}
}

int gpup_debug_dump( char* summary, int summarySize )
{
	FILE* f = nullptr;
	f = GGDiagFopen("gpup_dump.txt", "w");
	int loaded = 0;
	if ( f )
	{
		fprintf( f, "settings: tmr=%.6f gtimer=%.6f time=%.4f split=%d emitterCount=%d sn=%.2f rotsn=%.4f simulOn=%d\n",
			gpup_settings.tmr, gpup_settings.gtimer, gpup_settings.time, gpup_settings.split,
			gpup_settings.emitterCount, gpup_settings.sn, gpup_settings.rotsn, gpup_settings.simulOn );
		fprintf( f, "counters: doit_calls=%llu time_sum=%.2f max_time=%.3f arm_whole=%llu arm_split=%llu force_arm=%d draw_enable=%d agk_time=%.3f\n",
			(unsigned long long)gg_gpup_doit_calls, gg_gpup_time_sum, gg_gpup_max_time,
			(unsigned long long)gg_gpup_arm_whole, (unsigned long long)gg_gpup_arm_split,
			gg_gpup_force_arm, gg_gpup_draw_enable, AGKTimer() );
		{
			char ringbuf[512] = { 0 };
			wi::graphics::GG_GetDescriptorRingStats( ringbuf, sizeof(ringbuf) );
			fprintf( f, "%s\n", ringbuf );
		}
		// process-lifetime input textures — CRCs are PNG-derived constants; any drift = poisoned
		GG_DumpTextureStats( f, "noiseOrig", texNoiseOrig );
		GG_DumpTextureStats( f, "dist2", texDist2 );
		// process-lifetime static buffers — byte-compared against their in-exe source arrays
		// (round-2 prime suspects: the sim was proven healthy inside a caught white-out, so
		// the draw is the victim, and these are its only process-lifetime inputs)
		GG_CheckStaticBuffer( f, "ib1024", mainIndexBufferObj1, gpup_1024_indices, sizeof(unsigned short) * 6144 );
		GG_CheckStaticBuffer( f, "ib4096", mainIndexBufferObj0, gpup_4096_indices, sizeof(unsigned short) * 24576 );
		GG_CheckStaticBuffer( f, "vb1024", mainVertexBufferObj1, gpup_1024_vertices, sizeof(GPUP_1024_Vertex) * 4096 );
		GG_CheckStaticBuffer( f, "vb4096", mainVertexBufferObj0, gpup_4096_vertices, sizeof(GPUP_4096_Vertex) * 16384 );
		GG_CheckStaticBuffer( f, "quadvb", quadVertexBuffer, g_VerticesQuad, sizeof(VertexQuad) * 6 );
		// draw-path CPU globals with compile-time expected values (a scribbled CPU global
		// would survive level reload exactly like a scribbled GPU resource)
		fprintf( f, "  draw-globals: ibcount0=%d (exp 24576) ibcount1=%d (exp 6144)\n",
			mainIndexCountObj0, mainIndexCountObj1 );
		for ( int i = 0; i < gpup_maxeffects; i++ )
		{
			if ( gpup_emitter[i].effectLoaded == 0 ) continue;
			loaded++;
			fprintf( f, "emitter %d: vis=%d particles=%d blend=%d type=%d lifespan=%.4f(var %.3f) size1=%.4f size2=%.4f sizer=%.4f psize=%.4f gsize=%.3f animspd=%.4f amount=%.4f maxed=%.3f activeT=%.4f pos=(%.1f,%.1f,%.1f)\n",
				i, gpup_emitter[i].effectVisible, gpup_emitter[i].particles, gpup_emitter[i].blendmode,
				gpup_emitter[i].emitter_type, gpup_emitter[i].lifespan, gpup_emitter[i].lifespan_variance,
				gpup_emitter[i].size1, gpup_emitter[i].size2, gpup_emitter[i].sizer, gpup_emitter[i].particleSize,
				gpup_emitter[i].globalSize, gpup_emitter[i].emitter_animation_speed, gpup_emitter[i].emitter_amount,
				gpup_emitter[i].maxed, gpup_emitter[i].activeTimer,
				gpup_emitter[i].globalx[0], gpup_emitter[i].globaly[0], gpup_emitter[i].globalz[0] );
			fprintf( f, "  spawn-gate: emiton=%d active=%d spawnint=%.4f testpos=%d subpos=%d pauser=%d noiseOff=(%.4f,%.4f) zaehler=%.2f spawnpos=(%.1f,%.1f) rnd=%.3f rnd2=%.3f moveit=(%.3f,%.3f,%.3f)\n",
				gpup_emitter[i].emiton, gpup_emitter[i].emitterActive, gpup_emitter[i].spawnint,
				gpup_emitter[i].testpos, gpup_getSubPositionCount( i ), gpup_settings.pauser,
				gpup_emitter[i].noiseOff1, gpup_emitter[i].noiseOff2, gpup_emitter[i].noiseZaehler,
				gpup_emitter[i].speedConstantData.spawnpos.x, gpup_emitter[i].speedConstantData.spawnpos.y,
				gpup_emitter[i].speedConstantData.rnd, gpup_emitter[i].speedConstantData.rnd2,
				gpup_emitter[i].posConstantData.moveit.x, gpup_emitter[i].posConstantData.moveit.y,
				gpup_emitter[i].posConstantData.moveit.z );
			fprintf( f, "  gpu-consts: pgrow=(%.5f,%.5f,%.5f) globalsize=%.3f pos.lifespan=(%.2f,%.4f) warp=%.4f gravity=(%.4f,%.4f,%.4f,%.4f)\n",
				gpup_emitter[i].mainVSConstantData.pgrow.x, gpup_emitter[i].mainVSConstantData.pgrow.y,
				gpup_emitter[i].mainVSConstantData.pgrow.z, gpup_emitter[i].mainVSConstantData.globalsize.x,
				gpup_emitter[i].posConstantData.lifespan.x, gpup_emitter[i].posConstantData.lifespan.y,
				gpup_emitter[i].posConstantData.warp,
				gpup_emitter[i].speedConstantData.gravity.x, gpup_emitter[i].speedConstantData.gravity.y,
				gpup_emitter[i].speedConstantData.gravity.z, gpup_emitter[i].speedConstantData.gravity.w );
			// FULL draw-constant structs (task #120 round 3): the pre/post-trigger diff came
			// back clean on everything above, but the PS constants (opacity/clr/moder) and
			// most VS fields were never printed — a latched multiplier here is exactly the
			// "identical data, 5x the output" signature. Print everything the draw consumes.
			fprintf( f, "  ps-consts: clr=(%.4f,%.4f,%.4f) moder=%.4f opacity=%.4f tile=(%.3f,%.3f,%.3f,%.3f) imgcnt=(%.2f,%.2f,%.2f) agk=%.2f\n",
				gpup_emitter[i].mainPSConstantData.clr.x, gpup_emitter[i].mainPSConstantData.clr.y,
				gpup_emitter[i].mainPSConstantData.clr.z, gpup_emitter[i].mainPSConstantData.moder,
				gpup_emitter[i].mainPSConstantData.opacity,
				gpup_emitter[i].mainPSConstantData.tile.x, gpup_emitter[i].mainPSConstantData.tile.y,
				gpup_emitter[i].mainPSConstantData.tile.z, gpup_emitter[i].mainPSConstantData.tile.w,
				gpup_emitter[i].mainPSConstantData.image_count.x, gpup_emitter[i].mainPSConstantData.image_count.y,
				gpup_emitter[i].mainPSConstantData.image_count.z, gpup_emitter[i].mainPSConstantData.agk_time );
			fprintf( f, "  vs-consts2: tilex=(%.3f,%.3f,%.3f,%.3f) particles=(%.1f,%.1f,%.1f,%.1f) ppos=(%.2f,%.2f,%.2f) area=(%.2f,%.2f,%.2f) rotat=(%.3f,%.3f,%.3f) rota=(%.3f,%.3f,%.3f) gsize3=(%.3f,%.3f,%.3f) grot=(%.3f,%.3f,%.3f) campos=(%.1f,%.1f,%.1f) gpos0=(%.1f,%.1f,%.1f,%.1f)\n",
				gpup_emitter[i].mainVSConstantData.tilex.x, gpup_emitter[i].mainVSConstantData.tilex.y,
				gpup_emitter[i].mainVSConstantData.tilex.z, gpup_emitter[i].mainVSConstantData.tilex.w,
				gpup_emitter[i].mainVSConstantData.particles.x, gpup_emitter[i].mainVSConstantData.particles.y,
				gpup_emitter[i].mainVSConstantData.particles.z, gpup_emitter[i].mainVSConstantData.particles.w,
				gpup_emitter[i].mainVSConstantData.ppos.x, gpup_emitter[i].mainVSConstantData.ppos.y,
				gpup_emitter[i].mainVSConstantData.ppos.z,
				gpup_emitter[i].mainVSConstantData.area.x, gpup_emitter[i].mainVSConstantData.area.y,
				gpup_emitter[i].mainVSConstantData.area.z,
				gpup_emitter[i].mainVSConstantData.rotat.x, gpup_emitter[i].mainVSConstantData.rotat.y,
				gpup_emitter[i].mainVSConstantData.rotat.z,
				gpup_emitter[i].mainVSConstantData.rota.x, gpup_emitter[i].mainVSConstantData.rota.y,
				gpup_emitter[i].mainVSConstantData.rota.z,
				gpup_emitter[i].mainVSConstantData.globalsize.x, gpup_emitter[i].mainVSConstantData.globalsize.y,
				gpup_emitter[i].mainVSConstantData.globalsize.z,
				gpup_emitter[i].mainVSConstantData.globalrot.x, gpup_emitter[i].mainVSConstantData.globalrot.y,
				gpup_emitter[i].mainVSConstantData.globalrot.z,
				gpup_emitter[i].mainVSConstantData.CameraPos.x, gpup_emitter[i].mainVSConstantData.CameraPos.y,
				gpup_emitter[i].mainVSConstantData.CameraPos.z,
				gpup_emitter[i].mainVSConstantData.globalpos[0].x, gpup_emitter[i].mainVSConstantData.globalpos[0].y,
				gpup_emitter[i].mainVSConstantData.globalpos[0].z, gpup_emitter[i].mainVSConstantData.globalpos[0].w );
			// per-emitter GPU state: noise regenerates every tick from noiseOrig (stats matter,
			// crc varies); pos/speed are the live sim ping-pong state (healthy steam keeps
			// moderate means and low 255-saturation; a poisoned pool shows extreme skew)
			fprintf( f, "  currImage=%d\n", gpup_emitter[i].currImage );
			GG_DumpTextureStats( f, "noise", gpup_emitter[i].texNoise );
			GG_DumpTextureStats( f, "pos", gpup_emitter[i].texPos[gpup_emitter[i].currImage] );
			GG_DumpTextureStats( f, "speed", gpup_emitter[i].texSpeed[gpup_emitter[i].currImage] );
			// round-3: the LAST un-audited draw inputs — per-effect file textures, static after
			// load and kept across level reloads by the effect cache. gradient_1 feeds the VS's
			// per-particle ALPHA + COLOR + SIZE-GRADIENT (alphaVarying/colVarying/sizegrad);
			// a scribble here = opaque white oversized quads = the exact white-out signature.
			GG_DumpTextureStats( f, "gradient1", gpup_emitter[i].gradient_1 );
			GG_DumpTextureStats( f, "imagex", gpup_emitter[i].imagex );
			GG_DumpTextureStats( f, "image1", gpup_emitter[i].image1 );
			GG_DumpTextureStats( f, "t_field", gpup_emitter[i].t_field );
		}
		fclose( f );
	}
	if ( summary && summarySize > 0 )
	{
		snprintf( summary, summarySize,
			"loaded=%d steps=%llu time_sum=%.1f max_time=%.3f arm_whole=%llu arm_split=%llu force=%d draw=%d",
			loaded, (unsigned long long)gg_gpup_doit_calls, gg_gpup_time_sum, gg_gpup_max_time,
			(unsigned long long)gg_gpup_arm_whole, (unsigned long long)gg_gpup_arm_split,
			gg_gpup_force_arm, gg_gpup_draw_enable );
		summary[summarySize - 1] = 0;
	}
	return loaded;
}

void gpup_update( float frameTime, wiGraphics::CommandList cmd )
{
	if ( !gpu_particles_initialised ) return;
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	GraphicsDevice* device = wiGraphics::GetDevice();
	device->EventBegin( "GPUParticles Update", cmd );
	
	gpup_settings.tmr = frameTime * g_fSlowParticleTime;

	// SetGamePaused holds the particles still (they are still drawn)
	if ( g_bGamePaused ) gpup_settings.tmr = 0.0f;
	
	gpup_settings.sn = gpup_settings.sn + gpup_settings.tmr*10.0f;
	gpup_settings.rotsn = gpup_settings.rotsn + gpup_settings.tmr;
	
	if ( gpup_settings.sn > 999999999.0f ) gpup_settings.sn = 0.0f;
	if ( gpup_settings.rotsn > 999999999.0f ) gpup_settings.rotsn = 0.0f;
	
	gpup_settings.gtimer = gpup_settings.gtimer + gpup_settings.tmr;


	if ( gpup_settings.gtimer > 0.015f )
	{
		gpup_settings.time = gpup_settings.gtimer / 0.015f;

		// will split particle updates across two frames if more than 2 emitters are loaded and fps is high enough
		// GGMAX 2026-08-08 (task #122 round 3, USER-REPORTED "provoke the shift every time by
		// dollying closer/further"): the split arm is RETIRED — whole-batch always. The
		// interleave was broken since the DBPro era: the split==0 loop has no break, so it
		// ticks ALL emitters while lastEmitter runs off to 9, and the split==1 frame ticks
		// NOTHING — yet BOTH frames consume a half-quantum (0.0075*time). Net: the sim
		// receives HALF of real time whenever the split arm engages. The arm is gated on
		// frame time < 7.143ms (>140 FPS), which DX11's renderer never sustained — dormant
		// there for two decades — but the DX12 renderer sits right on that boundary
		// (116-141 FPS at the spotshadowtest steam pose), so camera distance (overdraw)
		// flips the steam between full-speed and HALF-SPEED regimes: the user's provokable
		// shape shift, captured by tracker11 (provoke 3: masked diff 67.5 vs noise 22;
		// e8 pool alpha 50.8->69.1 across one dolly). Whole-batch for 9-45 emitters is
		// negligible on 2026 GPUs; behavior is now FPS-invariant. SET_GPUPARM 2 still
		// forces the (broken, heritage) split arm for A/B archaeology.
		bool gg_whole_arm = true;
		if (gg_gpup_force_arm == 1) gg_whole_arm = true;
		else if (gg_gpup_force_arm == 2 && gpup_settings.emitterCount >= 2) gg_whole_arm = false;
		if (gg_whole_arm) gg_gpup_arm_whole++; else gg_gpup_arm_split++;
		if ( gg_whole_arm )
		{
			gpup_settings.activeEffects1 = 0;
			gpup_settings.activeEffects2 = 0;
			for( int i = 0; i < gpup_maxeffects; i++ )
			{
				if ( gpup_emitter[i].effectLoaded == 1 ) gpup_doit( i, cmd );
			}
			gpup_settings.gtimer = gpup_settings.gtimer - 0.015f*gpup_settings.time;
		}
		else
		{
			gpup_settings.split = 1 - gpup_settings.split;
			if ( gpup_settings.split == 0 )
			{
				gpup_settings.activeEffects1 = 0;
				int count = 0;
				for( int i = 0; i < gpup_maxeffects; i++ )
				{
					if ( gpup_emitter[i].effectLoaded == 1 ) 
					{
						gpup_doit( i, cmd );
						count++;
						if ( count >= floor(gpup_settings.emitterCount/2) ) 
						{
							gpup_settings.lastEmitter = i+1;
						}
					}
				}
			}
			else
			{
				gpup_settings.activeEffects2 = 0;
				int count = 0;
				for( int i = gpup_settings.lastEmitter; i < gpup_maxeffects; i++ )
				{
					if ( gpup_emitter[i].effectLoaded == 1 ) gpup_doit( i, cmd );
				}
			}
			gpup_settings.gtimer = gpup_settings.gtimer - 0.0075f*gpup_settings.time;
		}
		gpup_settings.activeEffects = gpup_settings.activeEffects1 + gpup_settings.activeEffects2;
	}

	device->EventEnd( cmd );
}

} // namespace GPUParticles
