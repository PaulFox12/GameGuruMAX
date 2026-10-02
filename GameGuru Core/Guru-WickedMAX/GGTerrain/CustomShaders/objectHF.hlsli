#ifndef WI_OBJECTSHADER_HF
#define WI_OBJECTSHADER_HF

#ifdef TRANSPARENT
#define DISABLE_TRANSPARENT_SHADOWMAP
#endif

#ifdef PLANARREFLECTION
#define DISABLE_ENVMAPS
#define DISABLE_VOXELGI
#endif

#ifdef WATER
#define DISABLE_VOXELGI
#endif

#define LIGHTMAP_QUALITY_BICUBIC


#include "globals.hlsli"
#include "brdf.hlsli"
#include "lightingHF.hlsli"
#include "GGDecalHF.hlsli"

// DEFINITIONS
//////////////////

#ifdef BINDLESS
Texture2D<float4> bindless_textures[] : register(t0, space1);
SamplerState bindless_samplers[] : register(t0, space2);
ByteAddressBuffer bindless_buffers[] : register(t0, space3);
PUSHCONSTANT(push, ObjectPushConstants);

inline ShaderMesh GetMesh()
{
	return bindless_buffers[push.mesh].Load<ShaderMesh>(0);
}
inline ShaderMaterial GetMaterial()
{
	return bindless_buffers[push.material].Load<ShaderMaterial>(0);
}
inline ShaderMaterial GetMaterial1()
{
	return bindless_buffers[GetMesh().blendmaterial1].Load<ShaderMaterial>(0);
}
inline ShaderMaterial GetMaterial2()
{
	return bindless_buffers[GetMesh().blendmaterial2].Load<ShaderMaterial>(0);
}
inline ShaderMaterial GetMaterial3()
{
	return bindless_buffers[GetMesh().blendmaterial3].Load<ShaderMaterial>(0);
}

#define sampler_objectshader			bindless_samplers[g_xFrame_ObjectShaderSamplerIndex]

#define texture_basecolormap			bindless_textures[GetMaterial().texture_basecolormap_index]
#define texture_normalmap				bindless_textures[GetMaterial().texture_normalmap_index]
#define texture_surfacemap				bindless_textures[GetMaterial().texture_surfacemap_index]
#define texture_emissivemap				bindless_textures[GetMaterial().texture_emissivemap_index]
#define texture_displacementmap			bindless_textures[GetMaterial().texture_displacementmap_index]
#define texture_occlusionmap			bindless_textures[GetMaterial().texture_occlusionmap_index]
#define texture_transmissionmap			bindless_textures[GetMaterial().texture_transmissionmap_index]
#define texture_sheencolormap			bindless_textures[GetMaterial().texture_sheencolormap_index]
#define texture_sheenroughnessmap		bindless_textures[GetMaterial().texture_sheenroughnessmap_index]
#define texture_clearcoatmap			bindless_textures[GetMaterial().texture_clearcoatmap_index]
#define texture_clearcoatroughnessmap	bindless_textures[GetMaterial().texture_clearcoatroughnessmap_index]
#define texture_clearcoatnormalmap		bindless_textures[GetMaterial().texture_clearcoatnormalmap_index]
#define texture_specularmap				bindless_textures[GetMaterial().texture_specularmap_index]

#define texture_blend1_basecolormap		bindless_textures[GetMaterial1().texture_basecolormap_index]
#define texture_blend1_normalmap		bindless_textures[GetMaterial1().texture_normalmap_index]
#define texture_blend1_surfacemap		bindless_textures[GetMaterial1().texture_surfacemap_index]
#define texture_blend1_emissivemap		bindless_textures[GetMaterial1().texture_emissivemap_index]

#define texture_blend2_basecolormap		bindless_textures[GetMaterial2().texture_basecolormap_index]
#define texture_blend2_normalmap		bindless_textures[GetMaterial2().texture_normalmap_index]
#define texture_blend2_surfacemap		bindless_textures[GetMaterial2().texture_surfacemap_index]
#define texture_blend2_emissivemap		bindless_textures[GetMaterial2().texture_emissivemap_index]

#define texture_blend3_basecolormap		bindless_textures[GetMaterial3().texture_basecolormap_index]
#define texture_blend3_normalmap		bindless_textures[GetMaterial3().texture_normalmap_index]
#define texture_blend3_surfacemap		bindless_textures[GetMaterial3().texture_surfacemap_index]
#define texture_blend3_emissivemap		bindless_textures[GetMaterial3().texture_emissivemap_index]


#else

inline ShaderMaterial GetMaterial()
{
	return g_xMaterial;
}
inline ShaderMaterial GetMaterial1()
{
	return g_xMaterial_blend1;
}
inline ShaderMaterial GetMaterial2()
{
	return g_xMaterial_blend2;
}
inline ShaderMaterial GetMaterial3()
{
	return g_xMaterial_blend3;
}

// These are bound by wiRenderer (based on Material):
TEXTURE2D(texture_basecolormap, float4, TEXSLOT_RENDERER_BASECOLORMAP);			// rgb: baseColor, a: opacity
TEXTURE2D(texture_normalmap, float3, TEXSLOT_RENDERER_NORMALMAP);				// rgb: normal
TEXTURE2D(texture_surfacemap, float4, TEXSLOT_RENDERER_SURFACEMAP);				// r: occlusion, g: roughness, b: metallic, a: reflectance
TEXTURE2D(texture_emissivemap, float4, TEXSLOT_RENDERER_EMISSIVEMAP);			// rgba: emissive
TEXTURE2D(texture_displacementmap, float, TEXSLOT_RENDERER_DISPLACEMENTMAP);	// r: heightmap
TEXTURE2D(texture_occlusionmap, float, TEXSLOT_RENDERER_OCCLUSIONMAP);			// r: occlusion
TEXTURE2D(texture_transmissionmap, float, TEXSLOT_RENDERER_TRANSMISSIONMAP);	// r: transmission factor
TEXTURE2D(texture_sheencolormap, float3, TEXSLOT_RENDERER_SHEENCOLORMAP);					// rgb
TEXTURE2D(texture_sheenroughnessmap, float4, TEXSLOT_RENDERER_SHEENROUGHNESSMAP);			// a
TEXTURE2D(texture_clearcoatmap, float, TEXSLOT_RENDERER_CLEARCOATMAP);						// r
TEXTURE2D(texture_clearcoatroughnessmap, float2, TEXSLOT_RENDERER_CLEARCOATROUGHNESSMAP);	// g
TEXTURE2D(texture_clearcoatnormalmap, float3, TEXSLOT_RENDERER_CLEARCOATNORMALMAP);			// rgb
TEXTURE2D(texture_specularmap, float4, TEXSLOT_RENDERER_SPECULARMAP);						// rgb color, a intensity

TEXTURE2D(texture_blend1_basecolormap, float4, TEXSLOT_RENDERER_BLEND1_BASECOLORMAP);	// rgb: baseColor, a: opacity
TEXTURE2D(texture_blend1_normalmap, float3, TEXSLOT_RENDERER_BLEND1_NORMALMAP);			// rgb: normal
TEXTURE2D(texture_blend1_surfacemap, float4, TEXSLOT_RENDERER_BLEND1_SURFACEMAP);		// r: occlusion, g: roughness, b: metallic, a: reflectance
TEXTURE2D(texture_blend1_emissivemap, float4, TEXSLOT_RENDERER_BLEND1_EMISSIVEMAP);		// rgba: emissive

TEXTURE2D(texture_blend2_basecolormap, float4, TEXSLOT_RENDERER_BLEND2_BASECOLORMAP);	// rgb: baseColor, a: opacity
TEXTURE2D(texture_blend2_normalmap, float3, TEXSLOT_RENDERER_BLEND2_NORMALMAP);			// rgb: normal
TEXTURE2D(texture_blend2_surfacemap, float4, TEXSLOT_RENDERER_BLEND2_SURFACEMAP);		// r: occlusion, g: roughness, b: metallic, a: reflectance
TEXTURE2D(texture_blend2_emissivemap, float4, TEXSLOT_RENDERER_BLEND2_EMISSIVEMAP);		// rgba: emissive

TEXTURE2D(texture_blend3_basecolormap, float4, TEXSLOT_RENDERER_BLEND3_BASECOLORMAP);	// rgb: baseColor, a: opacity
TEXTURE2D(texture_blend3_normalmap, float3, TEXSLOT_RENDERER_BLEND3_NORMALMAP);			// rgb: normal
TEXTURE2D(texture_blend3_surfacemap, float4, TEXSLOT_RENDERER_BLEND3_SURFACEMAP);		// r: occlusion, g: roughness, b: metallic, a: reflectance
TEXTURE2D(texture_blend3_emissivemap, float4, TEXSLOT_RENDERER_BLEND3_EMISSIVEMAP);		// rgba: emissive

#endif // BINDLESS


// These are bound by RenderPath (based on Render Path):
STRUCTUREDBUFFER(EntityTiles, uint, TEXSLOT_RENDERPATH_ENTITYTILES);
TEXTURE2D(texture_reflection, float4, TEXSLOT_RENDERPATH_REFLECTION);		// rgba: scene color from reflected camera angle
TEXTURE2D(texture_refraction, float4, TEXSLOT_RENDERPATH_REFRACTION);		// rgba: scene color from primary camera angle
// REMOVE_WATER_RIPPLE TEXTURE2D(texture_waterriples, float4, TEXSLOT_RENDERPATH_WATERRIPPLES);	// rgb: snorm8 water ripple normal map
TEXTURE2D(texture_ao, float, TEXSLOT_RENDERPATH_AO);						// r: ambient occlusion
TEXTURE2D(texture_ssr, float4, TEXSLOT_RENDERPATH_SSR);						// rgb: screen space ray-traced reflections, a: reflection blend based on ray hit or miss
TEXTURE2D(texture_rtshadow, uint4, TEXSLOT_RENDERPATH_RTSHADOW);			// bitmask for max 16 shadows' visibility

// Use these to compile this file as shader prototype:
//#define OBJECTSHADER_COMPILE_VS				- compile vertex shader prototype
//#define OBJECTSHADER_COMPILE_PS				- compile pixel shader prototype

// Use these to define the expected input layout for the shader:
//#define OBJECTSHADER_LAYOUT_POS				- input layout that has position and instance matrix
//#define OBJECTSHADER_LAYOUT_POS_TEX			- input layout that has position and texture coords
//#define OBJECTSHADER_LAYOUT_POS_PREVPOS		- input layout that has position and previous frame position
//#define OBJECTSHADER_LAYOUT_POS_PREVPOS_TEX	- input layout that has position, previous frame position and texture coords
//#define OBJECTSHADER_LAYOUT_COMMON			- input layout that has all the required inputs for common shaders

// Use these to define the expected input layout for the shader, but in a fine grained manner:
//	(These will not define additional capabilities)
//#define OBJECTSHADER_INPUT_POS				- adds position to input layout
//#define OBJECTSHADER_INPUT_PRE				- adds previous frame position to input layout
//#define OBJECTSHADER_INPUT_TEX				- adds texture coordinates to input layout
//#define OBJECTSHADER_INPUT_ATL				- adds atlas texture coords to input layout
//#define OBJECTSHADER_INPUT_COL				- adds vertex colors to input layout
//#define OBJECTSHADER_INPUT_TAN				- adds tangents to input layout

// Use these to enable features for the shader:
//	(Some of these are enabled automatically with OBJECTSHADER_LAYOUT defines)
//#define OBJECTSHADER_USE_CLIPPLANE				- shader will be clipped according to camera clip planes
//#define OBJECTSHADER_USE_WIND						- shader will use procedural wind animation
//#define OBJECTSHADER_USE_COLOR					- shader will use colors (material color, vertex color...)
//#define OBJECTSHADER_USE_DITHERING				- shader will use dithered transparency
//#define OBJECTSHADER_USE_UVSETS					- shader will sample textures with uv sets
//#define OBJECTSHADER_USE_ATLAS					- shader will use atlas
//#define OBJECTSHADER_USE_NORMAL					- shader will use normals
//#define OBJECTSHADER_USE_TANGENT					- shader will use tangents, normal mapping
//#define OBJECTSHADER_USE_POSITION3D				- shader will use world space positions
//#define OBJECTSHADER_USE_POSITIONPREV				- shader will use previous frame positions
//#define OBJECTSHADER_USE_EMISSIVE					- shader will use emissive
//#define OBJECTSHADER_USE_RENDERTARGETARRAYINDEX	- shader will use dynamic render target slice selection
//#define OBJECTSHADER_USE_NOCAMERA					- shader will not use camera space transform


#ifdef OBJECTSHADER_LAYOUT_POS // used by opaque shadows
#define OBJECTSHADER_INPUT_POS
#define OBJECTSHADER_USE_WIND
#endif // OBJECTSHADER_LAYOUT_POS

#ifdef OBJECTSHADER_LAYOUT_POS_TEX // used by shadows with alpha test or transparency
#define OBJECTSHADER_INPUT_POS
#define OBJECTSHADER_INPUT_TEX
#define OBJECTSHADER_USE_WIND
#define OBJECTSHADER_USE_UVSETS
#define OBJECTSHADER_USE_COLOR
#endif // OBJECTSHADER_LAYOUT_POS_TEX

#ifdef OBJECTSHADER_LAYOUT_POS_PREVPOS // used by depth prepass
#define OBJECTSHADER_INPUT_POS
#define OBJECTSHADER_INPUT_PRE
#define OBJECTSHADER_USE_CLIPPLANE
#define OBJECTSHADER_USE_WIND
#define OBJECTSHADER_USE_POSITIONPREV
#endif // OBJECTSHADER_LAYOUT_POS

#ifdef OBJECTSHADER_LAYOUT_POS_PREVPOS_TEX // used by depth prepass with alpha test or dithered transparency
#define OBJECTSHADER_INPUT_POS
#define OBJECTSHADER_INPUT_PRE
#define OBJECTSHADER_INPUT_TEX
#define OBJECTSHADER_USE_CLIPPLANE
#define OBJECTSHADER_USE_WIND
#define OBJECTSHADER_USE_POSITIONPREV
#define OBJECTSHADER_USE_UVSETS
#define OBJECTSHADER_USE_DITHERING
#endif // OBJECTSHADER_LAYOUT_POS_TEX

#ifdef OBJECTSHADER_LAYOUT_COMMON // used by common render passes
#define OBJECTSHADER_INPUT_POS
#define OBJECTSHADER_INPUT_TEX
#define OBJECTSHADER_INPUT_ATL
#define OBJECTSHADER_INPUT_COL
#define OBJECTSHADER_INPUT_TAN
#define OBJECTSHADER_USE_CLIPPLANE
#define OBJECTSHADER_USE_WIND
#define OBJECTSHADER_USE_UVSETS
#define OBJECTSHADER_USE_ATLAS
#define OBJECTSHADER_USE_COLOR
#define OBJECTSHADER_USE_NORMAL
#define OBJECTSHADER_USE_TANGENT
#define OBJECTSHADER_USE_POSITION3D
#define OBJECTSHADER_USE_EMISSIVE
#endif // OBJECTSHADER_LAYOUT_COMMON

#ifdef BINDLESS
static const uint instance_stride_matrix_userdata = 16 * 4;
static const uint instance_stride_atlas = 16;
static const uint instance_stride_prev = 16 * 3;

#ifdef OBJECTSHADER_INPUT_PRE
static const uint instance_stride = instance_stride_matrix_userdata + instance_stride_prev;
#else
#ifdef OBJECTSHADER_INPUT_ATL
static const uint instance_stride = instance_stride_matrix_userdata + instance_stride_atlas;
#else
static const uint instance_stride = instance_stride_matrix_userdata;
#endif // OBJECTSHADER_INPUT_ATL
#endif // OBJECTSHADER_INPUT_PRE
#endif // BINDLESS

struct VertexInput
{
#ifdef BINDLESS
	// Data coming from bindless fetching:
	uint vertexID : SV_VertexID;
	uint instanceID : SV_InstanceID;

	float4 GetPosition()
	{
		return float4(bindless_buffers[GetMesh().vb_pos_nor_wind].Load<float3>(vertexID * 16), 1);
	}
	float3 GetNormal()
	{
		const uint normal_wind = bindless_buffers[GetMesh().vb_pos_nor_wind].Load<uint4>(vertexID * 16).w;
		float3 normal;
		normal.x = (float)((normal_wind >> 0u) & 0xFF) / 255.0 * 2 - 1;
		normal.y = (float)((normal_wind >> 8u) & 0xFF) / 255.0 * 2 - 1;
		normal.z = (float)((normal_wind >> 16u) & 0xFF) / 255.0 * 2 - 1;
		return normal;
	}
	float GetWindWeight()
	{
		const uint normal_wind = bindless_buffers[GetMesh().vb_pos_nor_wind].Load<uint4>(vertexID * 16).w;
		return ((normal_wind >> 24u) & 0xFF) / 255.0;
	}

#ifdef OBJECTSHADER_INPUT_TEX
	float2 GetUV0()
	{
		[branch]
		if (GetMesh().vb_uv0 < 0)
			return 0;
		return unpack_half2(bindless_buffers[GetMesh().vb_uv0].Load<uint>(vertexID * 4));
	}
	float2 GetUV1()
	{
		[branch]
		if (GetMesh().vb_uv1 < 0)
			return 0;
		return unpack_half2(bindless_buffers[GetMesh().vb_uv1].Load<uint>(vertexID * 4));
	}
#endif // OBJECTSHADER_INPUT_TEX

	float4x4 GetInstanceMatrix()
	{
		float4 mat0 = bindless_buffers[push.instances].Load<float4>(push.instance_offset + instanceID * instance_stride + 16 * 0);
		float4 mat1 = bindless_buffers[push.instances].Load<float4>(push.instance_offset + instanceID * instance_stride + 16 * 1);
		float4 mat2 = bindless_buffers[push.instances].Load<float4>(push.instance_offset + instanceID * instance_stride + 16 * 2);
		return  float4x4(
			mat0,
			mat1,
			mat2,
			float4(0, 0, 0, 1)
			);
	}
	uint4 GetInstanceUserdata()
	{
		return bindless_buffers[push.instances].Load<uint4>(push.instance_offset + instanceID * instance_stride + 16 * 3);
	}

#ifdef OBJECTSHADER_INPUT_PRE
	float4 GetPositionPrev()
	{
		int descriptor_index = GetMesh().vb_pre;
		[branch]
		if (descriptor_index < 0)
		{
			descriptor_index = GetMesh().vb_pos_nor_wind;
		}
		return float4(bindless_buffers[descriptor_index].Load<float3>(vertexID * 16), 1);
	}
	float4x4 GetInstanceMatrixPrev()
	{
		float4 matPrev0 = bindless_buffers[push.instances].Load<float4>(push.instance_offset + instanceID * instance_stride + 16 * 4);
		float4 matPrev1 = bindless_buffers[push.instances].Load<float4>(push.instance_offset + instanceID * instance_stride + 16 * 5);
		float4 matPrev2 = bindless_buffers[push.instances].Load<float4>(push.instance_offset + instanceID * instance_stride + 16 * 6);
		return  float4x4(
			matPrev0,
			matPrev1,
			matPrev2,
			float4(0, 0, 0, 1)
			);
	}
#endif // OBJECTSHADER_INPUT_PRE

#ifdef OBJECTSHADER_INPUT_ATL
	float2 GetAtlasUV()
	{
		[branch]
		if (GetMesh().vb_atl < 0)
			return 0;
		return unpack_half2(bindless_buffers[GetMesh().vb_atl].Load<uint>(vertexID * 4));
	}
	float4 GetInstanceAtlas()
	{
		return bindless_buffers[push.instances].Load<float4>(push.instance_offset + instanceID * instance_stride + 16 * 4);
	}
#endif // OBJECTSHADER_INPUT_ATL

#ifdef OBJECTSHADER_INPUT_COL
	float4 GetVertexColor()
	{
		[branch]
		if (GetMesh().vb_col < 0)
			return 1;
		return unpack_rgba(bindless_buffers[GetMesh().vb_col].Load<uint>(vertexID * 4));
	}
#endif // OBJECTSHADER_INPUT_COL

#ifdef OBJECTSHADER_INPUT_TAN
	float4 GetTangent()
	{
		return unpack_utangent(bindless_buffers[GetMesh().vb_tan].Load<uint>(vertexID * 4)) * 2 - 1;
	}
#endif // OBJECTSHADER_INPUT_TAN





#else
	// Data coming from input layout:
	float4 pos : POSITION_NORMAL_WIND;
	float4 GetPosition()
	{
		return float4(pos.xyz, 1);
	}
	float3 GetNormal()
	{
		const uint normal_wind = asuint(pos.w);
		float3 normal;
		normal.x = (float)((normal_wind >> 0u) & 0xFF) / 255.0 * 2 - 1;
		normal.y = (float)((normal_wind >> 8u) & 0xFF) / 255.0 * 2 - 1;
		normal.z = (float)((normal_wind >> 16u) & 0xFF) / 255.0 * 2 - 1;
		return normal;
	}
	float GetWindWeight()
	{
		const uint normal_wind = asuint(pos.w);
		return ((normal_wind >> 24u) & 0xFF) / 255.0;
	}

#ifdef OBJECTSHADER_INPUT_PRE
	float4 pre : PREVPOS;
	float4 GetPositionPrev()
	{
		return float4(pre.xyz, 1);
	}
#endif // OBJECTSHADER_INPUT_PRE

#ifdef OBJECTSHADER_INPUT_TEX
	float2 uv0 : UVSET0;
	float2 uv1 : UVSET1;
	float2 GetUV0()
	{
		return uv0;
	}
	float2 GetUV1()
	{
		return uv1;
	}
#endif // OBJECTSHADER_INPUT_TEX

#ifdef OBJECTSHADER_INPUT_ATL
	float2 atl : ATLAS;
	float2 GetAtlasUV()
	{
		return atl;
	}
#endif // OBJECTSHADER_INPUT_ATL

#ifdef OBJECTSHADER_INPUT_COL
	float4 col : COLOR;
	float4 GetVertexColor()
	{
		return col;
	}
#endif // OBJECTSHADER_INPUT_COL

#ifdef OBJECTSHADER_INPUT_TAN
	float4 tan : TANGENT;
	float4 GetTangent()
	{
		return tan * 2 - 1;
	}
#endif // OBJECTSHADER_INPUT_TAN

	float4 mat0 : INSTANCEMATRIX0;
	float4 mat1 : INSTANCEMATRIX1;
	float4 mat2 : INSTANCEMATRIX2;
	uint4 userdata : INSTANCEUSERDATA;

#ifdef OBJECTSHADER_INPUT_PRE
	float4 matPrev0 : INSTANCEMATRIXPREV0;
	float4 matPrev1 : INSTANCEMATRIXPREV1;
	float4 matPrev2 : INSTANCEMATRIXPREV2;
	float4x4 GetInstanceMatrixPrev()
	{
		return  float4x4(
			matPrev0,
			matPrev1,
			matPrev2,
			float4(0, 0, 0, 1)
			);
	}
#endif // OBJECTSHADER_INPUT_PRE

#ifdef OBJECTSHADER_INPUT_ATL
	float4 atlasMulAdd : INSTANCEATLAS;
	float4 GetInstanceAtlas()
	{
		return atlasMulAdd;
	}
#endif // OBJECTSHADER_INPUT_ATL

	float4x4 GetInstanceMatrix()
	{
		return  float4x4(
			mat0,
			mat1,
			mat2,
			float4(0, 0, 0, 1)
			);
	}
	uint4 GetInstanceUserdata()
	{
		return userdata;
	}
#endif // BINDLESS
};


struct VertexSurface
{
	float4 position;
	float4 uvsets;
	float2 atlas;
	float4 color;
	float3 normal;
	float4 tangent;
	float4 positionPrev;
	uint emissiveColor;

	inline void create(in ShaderMaterial material, in VertexInput input)
	{
		float4x4 WORLD = input.GetInstanceMatrix();
		uint4 userdata = input.GetInstanceUserdata();
		position = input.GetPosition();
		color = material.baseColor * unpack_rgba(userdata.x);
		emissiveColor = userdata.z;

#ifdef OBJECTSHADER_INPUT_PRE
		positionPrev = input.GetPositionPrev();
#else
		positionPrev = position;
#endif // OBJECTSHADER_INPUT_PRE

#ifdef OBJECTSHADER_INPUT_COL
#ifdef OBJECTLOD
		//PE: We switch of textures and force vertexcolors.
		color *= input.GetVertexColor();
#else
		if (material.IsUsingVertexColors())
		{
			color *= input.GetVertexColor();
		}

#endif
#endif // OBJECTSHADER_INPUT_COL
		
		normal = normalize(mul((float3x3)WORLD, input.GetNormal()));

#ifdef OBJECTSHADER_INPUT_TAN
		tangent = input.GetTangent();
		tangent.xyz = normalize(mul((float3x3)WORLD, tangent.xyz));
#endif // OBJECTSHADER_INPUT_TAN

#ifdef OBJECTSHADER_INPUT_TEX
		uvsets = float4(input.GetUV0() * material.texMulAdd.xy + material.texMulAdd.zw, input.GetUV1());
#endif // OBJECTSHADER_INPUT_TEX

#ifdef OBJECTSHADER_INPUT_ATL
		float4 atlasMulAdd = input.GetInstanceAtlas();
		atlas = input.GetAtlasUV() * atlasMulAdd.xy + atlasMulAdd.zw;
#endif // OBJECTSHADER_INPUT_ATL

		position = mul(WORLD, position);

#ifdef OBJECTSHADER_INPUT_PRE
		positionPrev = mul(input.GetInstanceMatrixPrev(), positionPrev);
#else
		positionPrev = position;
#endif // OBJECTSHADER_INPUT_PRE

#ifdef OBJECTSHADER_USE_WIND
		if (material.IsUsingWind())
		{
			const float windweight = input.GetWindWeight();
			const float waveoffset = dot(position.xyz, g_xFrame_WindDirection) * g_xFrame_WindWaveSize + (position.x + position.y + position.z) * g_xFrame_WindRandomness;
			const float waveoffsetPrev = dot(positionPrev.xyz, g_xFrame_WindDirection) * g_xFrame_WindWaveSize + (positionPrev.x + positionPrev.y + positionPrev.z) * g_xFrame_WindRandomness;
			const float3 wavedir = g_xFrame_WindDirection * windweight;
			const float3 wind = sin(g_xFrame_Time * g_xFrame_WindSpeed + waveoffset) * wavedir;
			const float3 windPrev = sin(g_xFrame_TimePrev * g_xFrame_WindSpeed + waveoffsetPrev) * wavedir;
			position.xyz += wind;
			positionPrev.xyz += windPrev;
	}
#endif // OBJECTSHADER_USE_WIND
	}
};

struct PixelInput
{
	precise float4 pos : SV_POSITION;

#ifdef OBJECTSHADER_USE_CLIPPLANE
	float  clip : SV_ClipDistance0;
#endif // OBJECTSHADER_USE_CLIPPLANE

#ifdef OBJECTSHADER_USE_POSITIONPREV
	float4 pre : PREVIOUSPOSITION;
#endif // OBJECTSHADER_USE_POSITIONPREV

#ifdef OBJECTSHADER_USE_COLOR
	float4 color : COLOR;
#endif // OBJECTSHADER_USE_COLOR

#ifdef OBJECTSHADER_USE_DITHERING
	nointerpolation float dither : DITHER;
#endif // OBJECTSHADER_USE_DITHERING

#ifdef OBJECTSHADER_USE_UVSETS
	float4 uvsets : UVSETS;
#endif // OBJECTSHADER_USE_UVSETS

#ifdef OBJECTSHADER_USE_ATLAS
	float2 atl : ATLAS;
#endif // OBJECTSHADER_USE_ATLAS

#ifdef OBJECTSHADER_USE_NORMAL
	float3 nor : NORMAL;
#endif // OBJECTSHADER_USE_NORMAL

#ifdef OBJECTSHADER_USE_TANGENT
	float4 tan : TANGENT;
#endif // OBJECTSHADER_USE_TANGENT

#ifdef OBJECTSHADER_USE_POSITION3D
	float3 pos3D : WORLDPOSITION;
#endif // OBJECTSHADER_USE_POSITION3D

#ifdef OBJECTSHADER_USE_EMISSIVE
	uint emissiveColor : EMISSIVECOLOR;
#endif // OBJECTSHADER_USE_EMISSIVE

#ifdef OBJECTSHADER_USE_RENDERTARGETARRAYINDEX
#ifdef VPRT_EMULATION
	uint RTIndex : RTINDEX;
#else
	uint RTIndex : SV_RenderTargetArrayIndex;
#endif // VPRT_EMULATION
#endif // OBJECTSHADER_USE_RENDERTARGETARRAYINDEX
	
};

struct GBuffer
{
	float4 g0 : SV_TARGET0;	/*FORMAT_R11G11B10_FLOAT*/
	float4 g1 : SV_TARGET1;	/*FORMAT_R8G8B8A8_FLOAT*/
};
inline GBuffer CreateGBuffer(in float4 color, in Surface surface)
{
	GBuffer gbuffer;
	gbuffer.g0 = color;
#ifdef BRDF_CLEARCOAT
	gbuffer.g1 = float4(surface.clearcoat.N * 0.5f + 0.5f, surface.clearcoat.roughness);
#else
	gbuffer.g1 = float4(surface.N * 0.5f + 0.5f, surface.roughness);
#endif // BRDF_CLEARCOAT
	return gbuffer;
}


// METHODS
////////////

inline void ApplyEmissive(in Surface surface, inout Lighting lighting)
{
	lighting.direct.specular += surface.emissiveColor.rgb * surface.emissiveColor.a;
}

inline void LightMapping(in float2 ATLAS, inout Lighting lighting)
{
	[branch]
	if (any(ATLAS))
	{
#ifdef LIGHTMAP_QUALITY_BICUBIC
		lighting.indirect.diffuse = SampleTextureCatmullRom(texture_globallightmap, sampler_linear_clamp, ATLAS).rgb;
#else
		lighting.indirect.diffuse = texture_globallightmap.SampleLevel(sampler_linear_clamp, ATLAS, 0).rgb;
#endif // LIGHTMAP_QUALITY_BICUBIC
	}
}

inline void NormalMapping(in float4 uvsets, inout float3 N, in float3x3 TBN, out float3 bumpColor)
{
	[branch]
	if (GetMaterial().normalMapStrength > 0 && GetMaterial().uvset_normalMap >= 0)
	{
		const float2 UV_normalMap = GetMaterial().uvset_normalMap == 0 ? uvsets.xy : uvsets.zw;
		float3 normalMap = texture_normalmap.Sample(sampler_objectshader, UV_normalMap).rgb;
		normalMap.b = normalMap.b == 0 ? 1 : normalMap.b; // fix for missing blue channel
		bumpColor = normalMap.rgb * 2 - 1;
		N = normalize(lerp(N, mul(bumpColor, TBN), GetMaterial().normalMapStrength));
		bumpColor *= GetMaterial().normalMapStrength;
	}
	else
	{
		bumpColor = 0;
	}
}

#ifdef ROCKTRIPLANAR
// GG: Rock Triplanar (the custom shader of that name): the material's colour, normal and surface maps laid on in world
// space from three sides, as the terrain's Steep Rock lays its rock (GGTerrainRockHF.hlsli: the same sides, directions,
// two sizes and patches), so a cliff mesh with the same textures and tile size meets the terrain's rock without a seam.
// customShaderParam1 the tile size in metres (0: 30), customShaderParam2 how sharply the sides blend (1 as the terrain's,
// higher sharper; 0: 1), customShaderParam3 the variation, the texture at two sizes in patches (as Steep Rock Variation,
// 0 one size)
float RockObjectHash(float2 p)
{
	float3 p3 = frac(p.xyx * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return frac((p3.x + p3.y) * p3.z);
}

float RockObjectNoise(float2 p)
{
	float2 cell = floor(p);
	float2 f = frac(p);
	f = f * f * (3 - 2 * f);
	float a = RockObjectHash(cell);
	float b = RockObjectHash(cell + float2(1, 0));
	float c = RockObjectHash(cell + float2(0, 1));
	float d = RockObjectHash(cell + float2(1, 1));
	return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y) * 2 - 1;
}

// one side's sample by its weight: its UV and the world directions of its U and V (the normal map's X and Y)
void RockObjectSide(inout float3 colour, inout float4 surfaceMap, inout float3 normal, float weight, float2 uv, float2 uvDX, float2 uvDY, float3 dirU, float3 dirV, float3 N)
{
	float4 baseColorMap = texture_basecolormap.SampleGrad(sampler_objectshader, uv, uvDX, uvDY);
	colour += DEGAMMA(baseColorMap.rgb) * weight;
	float4 surface = 1;
	[branch]
	if (GetMaterial().uvset_surfaceMap >= 0)
	{
		surface = texture_surfacemap.SampleGrad(sampler_objectshader, uv, uvDX, uvDY);
	}
	surfaceMap += surface * weight;
	float3 tangentNormal = float3(0, 0, 1);
	[branch]
	if (GetMaterial().normalMapStrength > 0 && GetMaterial().uvset_normalMap >= 0)
	{
		tangentNormal = texture_normalmap.SampleGrad(sampler_objectshader, uv, uvDX, uvDY).rgb;
		tangentNormal.b = tangentNormal.b == 0 ? 1 : tangentNormal.b;
		tangentNormal = normalize(tangentNormal * 2 - 1);
	}
	dirU = normalize(dirU - N * dot(N, dirU));
	dirV = normalize(dirV - N * dot(N, dirV));
	normal += normalize(dirU * tangentNormal.x + dirV * tangentNormal.y + N * tangentNormal.z) * weight;
}

// the rock's colour (linear); its normal and surface map
float3 GGRockTriplanar(float3 P, float3 N, out float3 rockNormal, out float4 rockSurfaceMap)
{
	const float tile = GetMaterial().customShaderParam1 > 0 ? GetMaterial().customShaderParam1 : 30;
	const float sharpness = 4 * (GetMaterial().customShaderParam2 > 0 ? GetMaterial().customShaderParam2 : 1);
	const float rockScale = 1.0 / (tile * 39.37);
	float3 weights = pow(abs(N), sharpness);
	weights /= weights.x + weights.y + weights.z;
	const float3 posDX = ddx(P);
	const float3 posDY = ddy(P);

	float variation = 0;
	if (GetMaterial().customShaderParam3 > 0)
	{
		float2 vp = float2(P.x + P.y * 0.5, P.z - P.y * 0.5) * rockScale * 0.45 + 41.7;
		variation = smoothstep(0.35, 0.65, RockObjectNoise(vp) * 0.5 + 0.5) * saturate(GetMaterial().customShaderParam3);
	}

	float3 colour = 0;
	float3 normal = 0;
	rockSurfaceMap = 0;
	float sampled = 0;
	[unroll]
	for (int size = 0; size < 2; size++)
	{
		const float share = size == 0 ? 1 - variation : variation;
		[branch]
		if (share > 0.01)
		{
			const float scale = size == 0 ? rockScale : rockScale * 0.43;
			const float2 shift = size == 0 ? float2(0, 0) : float2(0.37, 0.71);
			[branch] if (weights.x > 0.01) RockObjectSide(colour, rockSurfaceMap, normal, weights.x * share, float2(P.z, -P.y) * scale + shift, float2(posDX.z, -posDX.y) * scale, float2(posDY.z, -posDY.y) * scale, float3(0, 0, 1), float3(0, -1, 0), N);
			[branch] if (weights.y > 0.01) RockObjectSide(colour, rockSurfaceMap, normal, weights.y * share, float2(P.x, -P.z) * scale + shift, float2(posDX.x, -posDX.z) * scale, float2(posDY.x, -posDY.z) * scale, float3(1, 0, 0), float3(0, 0, -1), N);
			[branch] if (weights.z > 0.01) RockObjectSide(colour, rockSurfaceMap, normal, weights.z * share, float2(P.x, -P.y) * scale + shift, float2(posDX.x, -posDX.y) * scale, float2(posDY.x, -posDY.y) * scale, float3(1, 0, 0), float3(0, -1, 0), N);
			sampled += ((weights.x > 0.01 ? weights.x : 0) + (weights.y > 0.01 ? weights.y : 0) + (weights.z > 0.01 ? weights.z : 0)) * share;
		}
	}
	colour /= sampled;
	rockSurfaceMap /= sampled;
	rockNormal = normalize(lerp(N, normalize(normal), saturate(GetMaterial().normalMapStrength)));
	return colour;
}
#endif // ROCKTRIPLANAR

inline float3 PlanarReflection(in Surface surface, in float2 bumpColor)
{
	float4 reflectionUV = mul(g_xCamera_ReflVP, float4(surface.P, 1));
	reflectionUV.xy /= reflectionUV.w;
	reflectionUV.xy = reflectionUV.xy * float2(0.5, -0.5) + 0.5;
	return texture_reflection.SampleLevel(sampler_linear_clamp, reflectionUV.xy/* + bumpColor*GetMaterial().normalMapStrength*/, 0).rgb; // bumpColor is causing an incorrect shift in the reflection
}

#define NUM_PARALLAX_OCCLUSION_STEPS 32
inline void ParallaxOcclusionMapping(inout float4 uvsets, in float3 V, in float3x3 TBN)
{
	[branch]
	if (GetMaterial().parallaxOcclusionMapping > 0 && GetMaterial().uvset_displacementMap >= 0)
	{
		V = mul(TBN, V);
		float layerHeight = 1.0 / NUM_PARALLAX_OCCLUSION_STEPS;
		float curLayerHeight = 0;
		float2 dtex = GetMaterial().parallaxOcclusionMapping * V.xy / NUM_PARALLAX_OCCLUSION_STEPS;
		float2 originalTextureCoords = GetMaterial().uvset_displacementMap == 0 ? uvsets.xy : uvsets.zw;
		float2 currentTextureCoords = originalTextureCoords;
		float2 derivX = ddx_coarse(currentTextureCoords);
		float2 derivY = ddy_coarse(currentTextureCoords);
		float heightFromTexture = 1 - texture_displacementmap.SampleGrad(sampler_linear_wrap, currentTextureCoords, derivX, derivY).r;
		uint iter = 0;
		[loop]
		while (heightFromTexture > curLayerHeight && iter < NUM_PARALLAX_OCCLUSION_STEPS)
		{
			curLayerHeight += layerHeight;
			currentTextureCoords -= dtex;
			heightFromTexture = 1 - texture_displacementmap.SampleGrad(sampler_linear_wrap, currentTextureCoords, derivX, derivY).r;
			iter++;
		}
		float2 prevTCoords = currentTextureCoords + dtex;
		float nextH = heightFromTexture - curLayerHeight;
		float prevH = 1 - texture_displacementmap.SampleGrad(sampler_linear_wrap, prevTCoords, derivX, derivY).r - curLayerHeight + layerHeight;
		float weight = nextH / (nextH - prevH);
		float2 finalTextureCoords = prevTCoords * weight + currentTextureCoords * (1.0 - weight);
		float2 difference = finalTextureCoords - originalTextureCoords;
		uvsets += difference.xyxy;
	}
}

inline void ForwardLighting(inout Surface surface, inout Lighting lighting)
{
#ifndef DISABLE_DECALS
	[branch]
	if (xForwardDecalMask != 0)
	{
		// decals are enabled, loop through them first:
		float4 decalAccumulation = 0;
		const float3 P_dx = ddx_coarse(surface.P);
		const float3 P_dy = ddy_coarse(surface.P);

		uint bucket_bits = xForwardDecalMask;

		[loop]
		while (bucket_bits != 0)
		{
			// Retrieve global entity index from local bucket, then remove bit from local bucket:
			const uint bucket_bit_index = firstbitlow(bucket_bits);
			const uint entity_index = bucket_bit_index;
			bucket_bits ^= 1u << bucket_bit_index;

			[branch]
			if (decalAccumulation.a < 1)
			{
				ShaderEntity decal = EntityArray[g_xFrame_DecalArrayOffset + entity_index];
				if ((decal.layerMask & surface.layerMask) == 0)
					continue;

				// Wicked's planar decal, or one of GameGuru's modes (GGDecalHF.hlsli):
				float edgeBlend;
				const float4 decalColor = GGDecalColor(decal, surface.P, P_dx, P_dy, edgeBlend);
				[branch]
				if (edgeBlend > 0)
				{
					// apply emissive:
					lighting.direct.specular += max(0, decalColor.rgb * decal.GetEmissive() * edgeBlend);
					// perform manual blending of decals:
					//  NOTE: they are sorted top-to-bottom, but blending is performed bottom-to-top
					decalAccumulation.rgb = (1 - decalAccumulation.a) * (decalColor.a*decalColor.rgb) + decalAccumulation.rgb;
					decalAccumulation.a = decalColor.a + (1 - decalColor.a) * decalAccumulation.a;
					[branch]
					if (decalAccumulation.a >= 1.0)
					{
						// force exit:
						break;
					}
				}
			}
			else
			{
				// force exit:
				break;
			}

		}

		surface.albedo.rgb = lerp(surface.albedo.rgb, decalAccumulation.rgb, decalAccumulation.a);
		GGDecalCoat(surface, decalAccumulation.a);
	}
#endif // DISABLE_DECALS


#ifndef DISABLE_ENVMAPS
	// Apply environment maps:
	float4 envmapAccumulation = 0;

#ifndef ENVMAPRENDERING
#ifndef DISABLE_LOCALENVPMAPS
	[branch]
	if (xForwardEnvProbeMask != 0)
	{
		uint bucket_bits = xForwardEnvProbeMask;

		[loop]
		while (bucket_bits != 0)
		{
			// Retrieve global entity index from local bucket, then remove bit from local bucket:
			const uint bucket_bit_index = firstbitlow(bucket_bits);
			const uint entity_index = bucket_bit_index;
			bucket_bits ^= 1u << bucket_bit_index;

			[branch]
			if (envmapAccumulation.a < 1)
			{
				ShaderEntity probe = EntityArray[g_xFrame_EnvProbeArrayOffset + entity_index];
				if ((probe.layerMask & surface.layerMask) == 0)
					continue;

				const float4x4 probeProjection = MatrixArray[probe.GetMatrixIndex()];
				const float3 clipSpacePos = mul(probeProjection, float4(surface.P, 1)).xyz;
				const float3 uvw = clipSpacePos.xyz * float3(0.5, -0.5, 0.5) + 0.5;
				[branch]
				if (is_saturated(uvw))
				{
					const float4 envmapColor = EnvironmentReflection_Local(surface, probe, probeProjection, clipSpacePos);
					// perform manual blending of probes:
					//  NOTE: they are sorted top-to-bottom, but blending is performed bottom-to-top
					envmapAccumulation.rgb = (1 - envmapAccumulation.a) * (envmapColor.a * envmapColor.rgb) + envmapAccumulation.rgb;
					envmapAccumulation.a = envmapColor.a + (1 - envmapColor.a) * envmapAccumulation.a;
					[branch]
					if (envmapAccumulation.a >= 1.0)
					{
						// force exit:
						break;
					}
				}
			}
			else
			{
				// force exit:
				break;
			}

		}
	}
#endif // DISABLE_LOCALENVPMAPS
#endif //ENVMAPRENDERING

	// Apply global envmap where there is no local envmap information:
	[branch]
	if (envmapAccumulation.a < 0.99)
	{
		envmapAccumulation.rgb = lerp(EnvironmentReflection_Global(surface), envmapAccumulation.rgb, envmapAccumulation.a);
	}
	lighting.indirect.specular += max(0, envmapAccumulation.rgb);
#endif // DISABLE_ENVMAPS

#ifndef DISABLE_VOXELGI
	VoxelGI(surface, lighting);
#endif //DISABLE_VOXELGI

	[branch]
	if (any(xForwardLightMask))
	{
		// Loop through light buckets for the draw call:
		const uint first_item = 0;
		const uint last_item = first_item + g_xFrame_LightArrayCount - 1;
		const uint first_bucket = first_item / 32;
		const uint last_bucket = min(last_item / 32, 1); // only 2 buckets max (uint2) for forward pass!
		[loop]
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = xForwardLightMask[bucket];

			[loop]
			while (bucket_bits != 0)
			{
				// Retrieve global entity index from local bucket, then remove bit from local bucket:
				const uint bucket_bit_index = firstbitlow(bucket_bits);
				const uint entity_index = bucket * 32 + bucket_bit_index;
				bucket_bits ^= 1u << bucket_bit_index;

				ShaderEntity light = EntityArray[g_xFrame_LightArrayOffset + entity_index];
				if ((light.layerMask & surface.layerMask) == 0)
					continue;

				if (light.GetFlags() & ENTITY_FLAG_LIGHT_STATIC)
				{
					continue; // static lights will be skipped (they are used in lightmap baking)
				}

				switch (light.GetType())
				{
				case ENTITY_TYPE_DIRECTIONALLIGHT:
				{
					DirectionalLight(light, surface, lighting);
				}
				break;
				case ENTITY_TYPE_POINTLIGHT:
				{
					PointLight(light, surface, lighting);
				}
				break;
#ifndef ENVMAPRENDERING
				case ENTITY_TYPE_SPOTLIGHT:
				{
					SpotLight(light, surface, lighting);
				}
				break;
#endif
				}
			}
		}
	}

}

inline void TiledLighting(inout Surface surface, inout Lighting lighting, out float3 envmapAmbient)
{
	envmapAmbient = 0;

	const uint2 tileIndex = uint2(floor(surface.pixel / TILED_CULLING_BLOCKSIZE));
	const uint flatTileIndex = flatten2D(tileIndex, g_xFrame_EntityCullingTileCount.xy) * SHADER_ENTITY_TILE_BUCKET_COUNT;

#ifndef DISABLE_DECALS
	[branch]
	if (g_xFrame_DecalArrayCount > 0)
	{
		// decals are enabled, loop through them first:
		float4 decalAccumulation = 0;
		const float3 P_dx = ddx_coarse(surface.P);
		const float3 P_dy = ddy_coarse(surface.P);

		// Loop through decal buckets in the tile:
		const uint first_item = g_xFrame_DecalArrayOffset;
		const uint last_item = first_item + g_xFrame_DecalArrayCount - 1;
		const uint first_bucket = first_item / 32;
		const uint last_bucket = min(last_item / 32, max(0, SHADER_ENTITY_TILE_BUCKET_COUNT - 1));
		[loop]
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = EntityTiles[flatTileIndex + bucket];

			// This is the wave scalarizer from Improved Culling - Siggraph 2017 [Drobot]:
			bucket_bits = WaveReadLaneFirst(WaveActiveBitOr(bucket_bits));

			[loop]
			while (bucket_bits != 0)
			{
				// Retrieve global entity index from local bucket, then remove bit from local bucket:
				const uint bucket_bit_index = firstbitlow(bucket_bits);
				const uint entity_index = bucket * 32 + bucket_bit_index;
				bucket_bits ^= 1u << bucket_bit_index;

				[branch]
				if (entity_index >= first_item && entity_index <= last_item && decalAccumulation.a < 1)
				{
					ShaderEntity decal = EntityArray[entity_index];
					if ((decal.layerMask & surface.layerMask) == 0)
						continue;

					// Wicked's planar decal, or one of GameGuru's modes (GGDecalHF.hlsli):
					float edgeBlend;
					const float4 decalColor = GGDecalColor(decal, surface.P, P_dx, P_dy, edgeBlend);
					[branch]
					if (edgeBlend > 0)
					{
						// apply emissive:
						lighting.direct.specular += max(0, decalColor.rgb * decal.GetEmissive() * edgeBlend);
						// perform manual blending of decals:
						//  NOTE: they are sorted top-to-bottom, but blending is performed bottom-to-top
						decalAccumulation.rgb = (1 - decalAccumulation.a) * (decalColor.a*decalColor.rgb) + decalAccumulation.rgb;
						decalAccumulation.a = decalColor.a + (1 - decalColor.a) * decalAccumulation.a;
						[branch]
						if (decalAccumulation.a >= 1.0)
						{
							// force exit:
							bucket = SHADER_ENTITY_TILE_BUCKET_COUNT;
							break;
						}
					}
				}
				else if (entity_index > last_item)
				{
					// force exit:
					bucket = SHADER_ENTITY_TILE_BUCKET_COUNT;
					break;
				}

			}
		}

		surface.albedo.rgb = lerp(surface.albedo.rgb, decalAccumulation.rgb, decalAccumulation.a);
		GGDecalCoat(surface, decalAccumulation.a);
	}
#endif // DISABLE_DECALS


#ifndef DISABLE_ENVMAPS
	// Apply environment maps:
	float4 envmapAccumulation = 0;

#ifndef DISABLE_LOCALENVPMAPS
	[branch]
	if (g_xFrame_EnvProbeArrayCount > 0)
	{
		// Loop through envmap buckets in the tile:
		const uint first_item = g_xFrame_EnvProbeArrayOffset;
		const uint last_item = first_item + g_xFrame_EnvProbeArrayCount - 1;
		const uint first_bucket = first_item / 32;
		const uint last_bucket = min(last_item / 32, max(0, SHADER_ENTITY_TILE_BUCKET_COUNT - 1));
		[loop]
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = EntityTiles[flatTileIndex + bucket];

			// Bucket scalarizer - Siggraph 2017 - Improved Culling [Michal Drobot]:
			bucket_bits = WaveReadLaneFirst(WaveActiveBitOr(bucket_bits));

			[loop]
			while (bucket_bits != 0)
			{
				// Retrieve global entity index from local bucket, then remove bit from local bucket:
				const uint bucket_bit_index = firstbitlow(bucket_bits);
				const uint entity_index = bucket * 32 + bucket_bit_index;
				bucket_bits ^= 1u << bucket_bit_index;

				[branch]
				if (entity_index >= first_item && entity_index <= last_item && envmapAccumulation.a < 1)
				{
					ShaderEntity probe = EntityArray[entity_index];
					if ((probe.layerMask & surface.layerMask) == 0)
						continue;

					const float4x4 probeProjection = MatrixArray[probe.GetMatrixIndex()];
					const float3 clipSpacePos = mul(probeProjection, float4(surface.P, 1)).xyz;
					const float3 uvw = clipSpacePos.xyz * float3(0.5, -0.5, 0.5) + 0.5;
					[branch]
					if (is_saturated(uvw))
					{
						float3 ambient = texture_envmaparray.SampleLevel(sampler_linear_clamp, float4(surface.N, probe.GetTextureIndex()), g_xFrame_EnvProbeMipCount-1).rgb;
						const float4 envmapColor = EnvironmentReflection_Local(surface, probe, probeProjection, clipSpacePos);
						// perform manual blending of probes:
						//  NOTE: they are sorted top-to-bottom, but blending is performed bottom-to-top
						envmapAccumulation.rgb = (1 - envmapAccumulation.a) * (envmapColor.a * envmapColor.rgb) + envmapAccumulation.rgb;
						envmapAmbient.rgb = (1 - envmapAccumulation.a) * (envmapColor.a * ambient.rgb) + envmapAmbient.rgb;
						envmapAccumulation.a = envmapColor.a + (1 - envmapColor.a) * envmapAccumulation.a;
						[branch]
						if (envmapAccumulation.a >= 1.0)
						{
							// force exit:
							bucket = SHADER_ENTITY_TILE_BUCKET_COUNT;
							break;
						}
					}
				}
				else if (entity_index > last_item)
				{
					// force exit:
					bucket = SHADER_ENTITY_TILE_BUCKET_COUNT;
					break;
				}

			}
		}
	}
#endif // DISABLE_LOCALENVPMAPS
	
	// Apply global envmap where there is no local envmap information:
	[branch]
	if (envmapAccumulation.a < 0.99)
	{
		float3 ambient = texture_envmaparray.SampleLevel(sampler_linear_clamp, float4(surface.N, g_xFrame_GlobalEnvProbeIndex), g_xFrame_EnvProbeMipCount-1).rgb;
		envmapAmbient = lerp( ambient, envmapAmbient, envmapAccumulation.a );
		envmapAccumulation.rgb = lerp(EnvironmentReflection_Global(surface), envmapAccumulation.rgb, envmapAccumulation.a);
	}
	lighting.indirect.specular += max(0, envmapAccumulation.rgb);
#endif // DISABLE_ENVMAPS

#ifndef DISABLE_VOXELGI
	VoxelGI(surface, lighting);
#endif //DISABLE_VOXELGI

	[branch]
	if (g_xFrame_LightArrayCount > 0)
	{
		uint4 shadow_mask_packed = 0;
#ifdef SHADOW_MASK_ENABLED
		[branch]
		if (g_xFrame_Options & OPTION_BIT_SHADOW_MASK)
		{
			shadow_mask_packed = texture_rtshadow[surface.pixel / 2];
		}
#endif // SHADOW_MASK_ENABLED

		// Loop through light buckets in the tile:
		const uint first_item = g_xFrame_LightArrayOffset;
		const uint last_item = first_item + g_xFrame_LightArrayCount - 1;
		const uint first_bucket = first_item / 32;
		const uint last_bucket = min(last_item / 32, max(0, SHADER_ENTITY_TILE_BUCKET_COUNT - 1));
		[loop]
		for (uint bucket = first_bucket; bucket <= last_bucket; ++bucket)
		{
			uint bucket_bits = EntityTiles[flatTileIndex + bucket];

			// Bucket scalarizer - Siggraph 2017 - Improved Culling [Michal Drobot]:
			bucket_bits = WaveReadLaneFirst(WaveActiveBitOr(bucket_bits));

			[loop]
			while (bucket_bits != 0)
			{
				// Retrieve global entity index from local bucket, then remove bit from local bucket:
				const uint bucket_bit_index = firstbitlow(bucket_bits);
				const uint entity_index = bucket * 32 + bucket_bit_index;
				bucket_bits ^= 1u << bucket_bit_index;

				// Check if it is a light and process:
				[branch]
				if (entity_index >= first_item && entity_index <= last_item)
				{
					ShaderEntity light = EntityArray[entity_index];
					if ((light.layerMask & surface.layerMask) == 0)
						continue;

					if (light.GetFlags() & ENTITY_FLAG_LIGHT_STATIC)
					{
						continue; // static lights will be skipped (they are used in lightmap baking)
					}

					float shadow_mask = 1;
#ifdef SHADOW_MASK_ENABLED
					[branch]
					if (g_xFrame_Options & OPTION_BIT_SHADOW_MASK && light.IsCastingShadow())
					{
						uint shadow_index = entity_index - g_xFrame_LightArrayOffset;
						if (shadow_index < 16)
						{
							uint mask_shift = (shadow_index % 4) * 8;
							uint mask_bucket = shadow_index / 4;
							uint mask = (shadow_mask_packed[mask_bucket] >> mask_shift) & 0xFF;
							if (mask == 0)
							{
								continue;
							}
							shadow_mask = mask / 255.0;
						}
					}
#endif // SHADOW_MASK_ENABLED

					switch (light.GetType())
					{
					case ENTITY_TYPE_DIRECTIONALLIGHT:
					{
						DirectionalLight(light, surface, lighting, shadow_mask);
					}
					break;
					case ENTITY_TYPE_POINTLIGHT:
					{
						PointLight(light, surface, lighting, shadow_mask);
					}
					break;
#ifndef ENVMAPRENDERING
					case ENTITY_TYPE_SPOTLIGHT:
					{
						SpotLight(light, surface, lighting, shadow_mask);
					}
					break;
#endif
					}
				}
				else if (entity_index > last_item)
				{
					// force exit:
					bucket = SHADER_ENTITY_TILE_BUCKET_COUNT;
					break;
				}

			}
		}
	}

}

inline void ApplyLighting(in Surface surface, in Lighting lighting, inout float4 color)
{
	LightingPart combined_lighting = CombineLighting(surface, lighting);
	color.rgb = lerp(surface.albedo * combined_lighting.diffuse, surface.refraction.rgb, surface.refraction.a) + combined_lighting.specular;
}

inline void ApplyFog(in float distance, float3 P, float3 V, inout float4 color)
{
    float fogMin = g_xFrame_Fog.x;
    float fogMax = g_xFrame_Fog.y;
    float3 fogColor = g_xFrame_WaterColor.rgb;
    float fogMinAmount = 1;

	/*if ( P.y < g_xFrame_WaterHeight && g_xCamera_CamPos.y < g_xFrame_WaterHeight ) */
    if ((g_xFrame_Options & OPTION_BIT_WATER_ENABLED) && P.y < g_xFrame_WaterHeight && g_xCamera_CamPos.y < g_xFrame_WaterHeight)
    {
        fogMin = g_xFrame_WaterFogMin;
        fogMax = g_xFrame_WaterFogMax;
        fogMinAmount = saturate(1 - g_xFrame_WaterFogMinAmount);

        float fogFade = V.y * 0.5 + 0.5;
        fogFade = 1.0 - (fogFade * fogFade);
        fogColor *= fogFade;

        float dist2 = max(0, g_xFrame_WaterHeight - P.y);
        distance += dist2;
    }
    else
    {

		//#ifdef GGREDUCED
		//LB: Restore fog control for interior scenes
        const float3 PassedInFogColor = GetFogColor();
        const float PassedInFogOpacity = clamp(GetFogOpacity(), 0.0, 1.0);
		//#endif

        if (g_xFrame_Options & OPTION_BIT_REALISTIC_SKY)
        {
            float3 horizonDir = -V;
            float invLen = rsqrt(horizonDir.x * horizonDir.x + horizonDir.z * horizonDir.z);
            invLen *= 0.995;
            horizonDir.x *= invLen;
            horizonDir.z *= invLen;
            fogColor = GetDynamicSkyColor(float3(horizonDir.x, 0.1, horizonDir.z), false, false, false, true);
        }
        else
        {
            V = -V;
            V.y = abs(V.y);
			//PE: interior scenes , skybox looks strange, so disable for now.
			//fogColor = texture_globalenvmap.SampleLevel(sampler_linear_clamp, V, 8).rgb;
            fogColor = color.rgb;
			//#ifdef GGREDUCED
			//PE: Remove env map by opacity.
			//fogColor = lerp(color.rgb, fogColor, PassedInFogOpacity);
			//#endif
        }

		//#ifdef GGREDUCED
		//LB: Restore fog control for interior scenes
        fogColor = lerp(fogColor, PassedInFogColor, PassedInFogOpacity);
		//#endif
    }

    distance = max(0.0, distance - fogMin);
    distance = exp(distance * 4.0 / (fogMin - fogMax));
    distance = min(fogMinAmount, distance);
		
    color.rgb = lerp(fogColor, color.rgb, distance);

	/*
	const float fogAmount = GetFogAmount(distance, P, V);
	const float3 FogColor = GetFogColor();
	const float FogOpacity = GetFogOpacity();
	if (g_xFrame_Options & OPTION_BIT_REALISTIC_SKY)
	{
		const float3 skyLuminance = texture_skyluminancelut.SampleLevel(sampler_point_clamp, float2(0.5, 0.5), 0).rgb;
		const float3 finalFogColor = lerp(skyLuminance, FogColor, FogOpacity);
		color.rgb = lerp(color.rgb, finalFogColor, fogAmount);
		//color.rgb = lerp(color.rgb, skyLuminance, fogAmount);
	}
	else
	{
		const float3 V = float3(0.0, -1.0, 0.0);
		const float3 skyDynamicCol = GetDynamicSkyColor(V, false, false, false, true);
		const float3 finalFogColor = lerp(skyDynamicCol, FogColor, FogOpacity);
		color.rgb = lerp(color.rgb, finalFogColor, fogAmount);
		//color.rgb = lerp(color.rgb, GetDynamicSkyColor(V, false, false, false, true), fogAmount);
	}
	*/
}

// OBJECT SHADER PROTOTYPE
///////////////////////////

#ifdef OBJECTSHADER_COMPILE_VS

#ifdef TREEANIMNATE
//PE: Animate the trees a bit.
float TreeWaveX(float posy, float posx)
{
	float treeWind = g_xFrame_TreeWind;
	float materialWindParam = GetMaterial().customShaderParam1;
	float isZeroWind = 1.0 - step(0.000001, abs(treeWind));
	float isNonZeroWind = step(0.000001, abs(treeWind));
	float baseWind = (isZeroWind * materialWindParam) + (isNonZeroWind * treeWind);
	float wind = baseWind * materialWindParam;

	const float clamped = clamp((posy - 120) * 0.35, 0, posy);
    const float swayspeed = (g_xFrame_TreeWindSpeed > 0) ? g_xFrame_TreeWindSpeed * materialWindParam : wind * 6.0; // own speed (SetTreeWind's second value), else tied to the amount
    const float swayamount = wind * 0.35; //0.075
    const float time = g_xFrame_Time;
    const float sdat = sin((time * (swayspeed * 1.5)) + posx) + cos((time * (swayspeed * 0.8)) + posx) + sin((time * (swayspeed * 1.2)));
    const float wave = sdat * 0.335 * clamp((posy - 120) * 0.35, 0, 1);
    return (wave * (clamped * swayamount));
}
float TreeWaveZ(float posy, float posx)
{
	float treeWind = g_xFrame_TreeWind;
	float materialWindParam = GetMaterial().customShaderParam1;
	float isZeroWind = 1.0 - step(0.000001, abs(treeWind));
	float isNonZeroWind = step(0.000001, abs(treeWind));
	float baseWind = (isZeroWind * materialWindParam) + (isNonZeroWind * treeWind);
	float wind = baseWind * materialWindParam;

	const float clamped = clamp((posy - 120) * 0.35, 0, posy);
    const float swayspeed = (g_xFrame_TreeWindSpeed > 0) ? g_xFrame_TreeWindSpeed * materialWindParam : wind * 6.0; // own speed (SetTreeWind's second value), else tied to the amount
    const float swayamount = wind * 0.20; //0.055
    const float time = g_xFrame_Time;
    const float sdat = sin((time * swayspeed) + posx) + cos((time * (swayspeed * 1.5)) + posx);
    const float wave = sdat * 0.5 * clamp((posy - 120) * 0.35, 0, 1);
    return (wave * (clamped * swayamount));
}
#endif

// Vertex shader base:
PixelInput main(VertexInput input)
{
	PixelInput Out;

	float4 inputposition = input.GetPosition();
	VertexSurface surface;
	surface.create(GetMaterial(), input);

	Out.pos = surface.position;

#ifdef TREEANIMNATE
	float4 mpos = input.GetPosition();
	const float scaley = input.mat1[1]; //PE: input.mat1[1] = WORLD[1][1];

	//PE: rotation of the actual local position to get the real y is to slow.
	//const float4x4 WORLD = input.GetInstanceMatrix();
    //float3 localYOffset = float3(0, mpos.y, 0);
    //float3x3 worldRotationMatrix = float3x3(
    //    WORLD[0][0], WORLD[1][0], WORLD[2][0],
    //    WORLD[0][1], WORLD[1][1], WORLD[2][1],
    //    WORLD[0][2], WORLD[1][2], WORLD[2][2]
    //);
    //float3 rotatedLocalY = mul(localYOffset, worldRotationMatrix);
	//float yScale = length(float3(WORLD[0][1], WORLD[1][1], WORLD[2][1]));
	//mpos.y = rotatedLocalY.y;

	mpos.y *= scaley;
    Out.pos.x += TreeWaveX(mpos.y,  input.mat0[3] + input.mat2[3] );
    Out.pos.z += TreeWaveZ(mpos.y, input.mat0[3] );
#endif

#ifndef OBJECTSHADER_USE_NOCAMERA
	Out.pos = mul(g_xCamera_VP, Out.pos);
#endif // OBJECTSHADER_USE_NOCAMERA

#ifdef OBJECTSHADER_USE_CLIPPLANE
	Out.clip = dot(surface.position, g_xCamera_ClipPlane);
#endif // OBJECTSHADER_USE_CLIPPLANE

#ifdef OBJECTSHADER_USE_POSITIONPREV
	Out.pre = surface.positionPrev;
#ifndef OBJECTSHADER_USE_NOCAMERA
	Out.pre = mul(g_xCamera_PrevVP, Out.pre);
#endif // OBJECTSHADER_USE_NOCAMERA
#endif // OBJECTSHADER_USE_POSITIONPREV

#ifdef OBJECTSHADER_USE_POSITION3D
	Out.pos3D = surface.position.xyz;
#endif // OBJECTSHADER_USE_POSITION3D

#ifdef OBJECTSHADER_USE_COLOR
	Out.color = surface.color;
#endif // OBJECTSHADER_USE_COLOR

#ifdef OBJECTSHADER_USE_DITHERING
	Out.dither = surface.color.a;
#endif // OBJECTSHADER_USE_DITHERING

#ifdef OBJECTSHADER_USE_UVSETS
	Out.uvsets = surface.uvsets;
#endif // OBJECTSHADER_USE_UVSETS

#ifdef OBJECTSHADER_USE_ATLAS
	Out.atl = surface.atlas;
#endif // OBJECTSHADER_USE_ATLAS

#ifdef OBJECTSHADER_USE_NORMAL
	Out.nor = surface.normal;
#endif // OBJECTSHADER_USE_NORMAL

#ifdef OBJECTSHADER_USE_TANGENT
	Out.tan = surface.tangent;
#endif // OBJECTSHADER_USE_TANGENT

#ifdef OBJECTSHADER_USE_EMISSIVE
	Out.emissiveColor = surface.emissiveColor;
#endif // OBJECTSHADER_USE_EMISSIVE

#ifdef OBJECTSHADER_USE_RENDERTARGETARRAYINDEX
	const uint frustum_index = input.GetInstanceUserdata().y;
	Out.RTIndex = xCubemapRenderCams[frustum_index].properties.x;
#ifndef OBJECTSHADER_USE_NOCAMERA
	Out.pos = mul(xCubemapRenderCams[frustum_index].VP, surface.position);
#endif // OBJECTSHADER_USE_NOCAMERA
#endif // OBJECTSHADER_USE_RENDERTARGETARRAYINDEX

	return Out;
}

#endif // OBJECTSHADER_COMPILE_VS



#ifdef OBJECTSHADER_COMPILE_PS

// Possible switches:
//	OUTPUT_GBUFFER		-	assemble object shader for gbuffer exporting
//	PREPASS				-	assemble object shader for depth prepass rendering
//	TRANSPARENT			-	assemble object shader for forward or tile forward transparent rendering
//	ENVMAPRENDERING		-	modify object shader for envmap rendering
//	PLANARREFLECTION	-	include planar reflection sampling
//	POM					-	include parallax occlusion mapping computation
//	WATER				-	include specialized water shader code
//	TERRAIN				-	include specialized terrain material blending code

struct OutputPrepass
	{
		float4 velocity : SV_TARGET0;
		uint   readback : SV_TARGET1;  // virtual texture read back
	};

#ifdef DISABLE_ALPHATEST
[earlydepthstencil]
#endif // DISABLE_ALPHATEST


// entry point:
#ifdef PREPASS
	OutputPrepass main(PixelInput input)
#else
  #ifdef OUTPUT_GBUFFER
	GBuffer main(PixelInput input, in bool is_frontface : SV_IsFrontFace)
  #else
	float4 main(PixelInput input, in bool is_frontface : SV_IsFrontFace) : SV_TARGET
  #endif // OUTPUT_GBUFFER
#endif // PREPASS

// Pixel shader base:
{
	const float depth = input.pos.z;
	const float lineardepth = input.pos.w;
	const float2 pixel = input.pos.xy;
	const float2 ScreenCoord = pixel * g_xFrame_InternalResolution_rcp;

#ifdef RIVERWATER
	// GG: a river's water is drawn only in the main view, not in the passes with a clip plane (the transparents under the
	// sea drawn before the refraction copy, the planar reflection), so its refraction is the scene beneath it
	if (any(g_xCamera_ClipPlane != 0)) discard;
#endif // RIVERWATER
	float3 bumpColor = 0;

#ifndef DISABLE_ALPHATEST
#ifndef TRANSPARENT
#ifndef ENVMAPRENDERING
#ifdef OBJECTSHADER_USE_DITHERING
	// apply dithering:
	clip(dither(pixel + GetTemporalAASampleRotation()) - (1 - input.dither));
#endif // OBJECTSHADER_USE_DITHERING
#endif // DISABLE_ALPHATEST
#endif // TRANSPARENT
#endif // ENVMAPRENDERING


#ifdef OBJECTSHADER_USE_POSITIONPREV
	float2 pos2D = ScreenCoord * 2 - 1;
	pos2D.y *= -1;
	input.pre.xy /= input.pre.w;
	const float2 velocity = ((input.pre.xy - g_xFrame_TemporalAAJitterPrev) - (pos2D.xy - g_xFrame_TemporalAAJitter)) * float2(0.5, -0.5);
#endif // OBJECTSHADER_USE_POSITIONPREV


	Surface surface;
	surface.init();


#ifdef OBJECTSHADER_USE_NORMAL
	if (is_frontface == false)
	{
		input.nor = -input.nor;
	}
	surface.N = normalize(input.nor);
	decal_faceN = surface.N;
#endif // OBJECTSHADER_USE_NORMAL

#ifdef OBJECTSHADER_USE_POSITION3D
	surface.P = input.pos3D;
	surface.V = g_xCamera_CamPos - surface.P;
	float dist = length(surface.V);
	surface.V /= dist;
#endif // OBJECTSHADER_USE_POSITION3D

#ifdef OBJECTSHADER_USE_TANGENT
#if 0
	float3x3 TBN = compute_tangent_frame(surface.N, surface.P, input.uvsets.xy);
#else
	float4 tangent = input.tan;
	tangent.xyz = normalize(tangent.xyz);
	float3 binormal = normalize(cross(tangent.xyz, surface.N) * tangent.w);
	float3x3 TBN = float3x3(tangent.xyz, binormal, surface.N);
#endif

#ifdef POM
	ParallaxOcclusionMapping(input.uvsets, surface.V, TBN);
#endif // POM

#endif // OBJECTSHADER_USE_TANGENT



	float4 color = 1;

#ifdef WATEROBJECT
#ifdef RIVERWATER

	// GG: a river's water. It flows down the first texture coordinates' v at this point's speed (the second coordinates'
	// y, 1 the river's base speed), the normal map sampled twice half a cycle apart and cross-faded, so a fast stretch never
	// smears it (two-phase flow), each cycle starting somewhere new so the reset doesn't pulse, and a larger, slower pair
	// under it breaks the repeat. Turbulence (the second coordinates' x) makes it choppier. Foam is the foam texture (the
	// base colour map) dissolved by a mask, the turbulence and, by customShaderParam6, the shore
	const float riverTurbulence = saturate(input.uvsets.z);
	const float riverSpeed = max(0.0, input.uvsets.w);
	const float flowRate = 0.03 * GetMaterial().customShaderParam2 * riverSpeed; // texture tiles a second
	const float phaseRate = 0.35; // cycles a second
	const float phase0 = frac(g_xFrame_Time * phaseRate);
	const float phase1 = frac(g_xFrame_Time * phaseRate + 0.5);
	const float weight0 = 1 - abs(2 * phase0 - 1);
	const float travel = flowRate / phaseRate;
	const float2 jump0 = frac(floor(g_xFrame_Time * phaseRate) * float2(0.3713, 0.6197));
	const float2 jump1 = frac(floor(g_xFrame_Time * phaseRate + 0.5) * float2(0.5281, 0.2879) + 0.5);
	const float2 riverUV = input.uvsets.xy * GetMaterial().customShaderParam1;
	const float2 uvA = riverUV + jump0 - float2(0, travel * phase0);
	const float2 uvB = riverUV + jump1 - float2(0, travel * phase1);
	const float2 uvC = riverUV * 0.37 + jump0.yx - float2(0, travel * 0.6 * 0.37 * phase0);
	const float2 uvD = riverUV * 0.37 + jump1.yx - float2(0, travel * 0.6 * 0.37 * phase1);

	const float3 riverN = surface.N;
	float3 nA = riverN, nB = riverN, nC = riverN, nD = riverN;
	float3 bA = 0, bB = 0, bC = 0, bD = 0;
	float4 riverSets = input.uvsets;
	riverSets.xy = uvA; NormalMapping(riverSets, nA, TBN, bA);
	riverSets.xy = uvB; NormalMapping(riverSets, nB, TBN, bB);
	riverSets.xy = uvC; NormalMapping(riverSets, nC, TBN, bC);
	riverSets.xy = uvD; NormalMapping(riverSets, nD, TBN, bD);
	const float3 nFine = normalize(nA * weight0 + nB * (1 - weight0));
	const float3 nBroad = normalize(nC * weight0 + nD * (1 - weight0));
	surface.N = normalize(lerp(nFine, nBroad, 0.35));
	surface.N = normalize(lerp(riverN, surface.N, min(3.0, (1 + 1.5 * riverTurbulence) * GetMaterial().customShaderParam3)));
	bumpColor = lerp(bA * weight0 + bB * (1 - weight0), bC * weight0 + bD * (1 - weight0), 0.35) * (1 + 1.5 * riverTurbulence);

	// the foam: the foam texture along the flow at two scales, dissolved where the mask is low
	float foamTex = lerp(texture_basecolormap.Sample(sampler_objectshader, uvB * 1.7).r, texture_basecolormap.Sample(sampler_objectshader, uvA * 1.7).r, weight0);
	foamTex = lerp(foamTex, lerp(texture_basecolormap.Sample(sampler_objectshader, uvD * 2.3).r, texture_basecolormap.Sample(sampler_objectshader, uvC * 2.3).r, weight0), 0.4);
	const float shoreDepth = texture_lineardepth.SampleLevel(sampler_point_clamp, ScreenCoord.xy, 0) * g_xCamera_ZFarP - lineardepth;
	const float shore = saturate(1 - shoreDepth / 40.0) * saturate(GetMaterial().customShaderParam6);
	const float foamMask = max(riverTurbulence, shore);
	float riverFoam = saturate((foamTex - (1 - foamMask)) * 3.0) * saturate(foamMask * 4.0);

#else

	float uvscale = GetMaterial().customShaderParam1;
	float uvspeed = GetMaterial().customShaderParam2;
	float uvdistorsion = GetMaterial().customShaderParam3;
	float uvdirection = GetMaterial().customShaderParam4;
	float uvscroll = 0.03f * GetMaterial().customShaderParam5;
	float2 newuv = input.uvsets.xy * uvscale;

	float distortion = 0.0085f * uvdistorsion; // PE: water distortion reflection default 0.0055f
	float distortion2 = 0.040f * uvdistorsion; // PE: water distortion waves default 0.030f
	float WaterSpeed1 = 0.03f * uvspeed; // PE: Speed 1 lower = faster default 30.0f
	float WaterSpeed2 = 0.0125f * uvspeed; // PE: Speed 2 lower = faster default 70.0f  

    float angle = uvdirection * 2.0 * PI;
    float2 direction;
    direction.x = cos(angle);
    direction.y = sin(angle);
	float2 offset = direction * WaterSpeed1 * g_xFrame_Time;
	float2 offset2 = direction * uvscroll * g_xFrame_Time;

	if ( uvdirection == 0)
	{
		offset = 0;
		offset2 = 0;
	}
	newuv += offset;

	float3 normalwatermap = texture_normalmap.Sample(sampler_objectshader, newuv);
	float3 normalwatermap3 = texture_normalmap.Sample(sampler_objectshader, newuv+(g_xFrame_Time*WaterSpeed1));
	float genericwave = (normalwatermap.b-0.5*2.0)+0.5;
	float3 dudv = normalize( normalwatermap.rgb * 2.0 - 1.0) * distortion;
	float3 dudv2 = normalize( normalwatermap.rgb * 2.0 - 1.0) * distortion2;
	float3 dudv3 = normalize( normalwatermap3.rgb * 2.0 - 1.0) * distortion;

	float2 timeScaledUV = newuv+sin((g_xFrame_Time*WaterSpeed1));
	float2 timeScaledUV2 = (newuv*0.5)+dudv2.rg+cos((g_xFrame_Time*WaterSpeed2));
	float2 timeScaledUV3 = (newuv*0.25)+dudv2.rg+sin((g_xFrame_Time*WaterSpeed2));
	float2 timeScaledUV4 = ((input.uvsets.xy+offset)*8)+dudv2.rg+sin((g_xFrame_Time*WaterSpeed1*2));
	float2 timeScaledUV5 = ((input.uvsets.xy+offset)*6)+dudv.rg+sin((g_xFrame_Time*WaterSpeed2*2));
	float2 timeScaledUV6 = ((input.uvsets.xy+offset)*5)+dudv.rg+cos((g_xFrame_Time*WaterSpeed2*2));

	float4 baseColorMap = texture_basecolormap.Sample(sampler_objectshader, (input.uvsets.xy * GetMaterial().customShaderParam7 )+dudv3.rg+offset2);

	float3 bumpColor2 = 0, bumpColor3 = 0, bumpColor4 = 0, bumpColor5 = 0, bumpColor6 = 0;
	float3 normal2 = 0, normal3 = 0, normal4 = 0;
	float2 orguv = input.uvsets.xy;

	input.uvsets.xy = timeScaledUV;
	NormalMapping(input.uvsets, surface.N, TBN, bumpColor);
	bumpColor4 = bumpColor;
	input.uvsets.xy = timeScaledUV2;
	NormalMapping(input.uvsets, normal2, TBN, bumpColor2);
	input.uvsets.xy = timeScaledUV3;
	NormalMapping(input.uvsets, normal3, TBN, bumpColor3);
	input.uvsets.xy = orguv;

	surface.N = lerp(surface.N,normal2,0.5f);
	surface.N = lerp(surface.N,normal3,0.5f);
	bumpColor = lerp(bumpColor,bumpColor2,0.5f);
	bumpColor = lerp(bumpColor,bumpColor3,0.5f);

	float sampled_lineardepth = texture_lineardepth.SampleLevel(sampler_point_clamp, ScreenCoord.xy , 0) * g_xCamera_ZFarP;

	float depth_difference = sampled_lineardepth - lineardepth;
	float3 foamColor = float3(2.0, 2.0, 2.0); // Define your foam color (e.g., white)
	float foamThreshold = 4.0 * GetMaterial().customShaderParam6;
	
	float foamIntensity = saturate((foamThreshold - depth_difference) / foamThreshold);

	[branch]
    if (foamIntensity > 0.0)
    {
		input.uvsets.xy = timeScaledUV4;
		NormalMapping(input.uvsets, normal4, TBN, bumpColor4);
		input.uvsets.xy = timeScaledUV5;
		NormalMapping(input.uvsets, normal4, TBN, bumpColor5);
		input.uvsets.xy = timeScaledUV6;
		NormalMapping(input.uvsets, normal4, TBN, bumpColor6);
		bumpColor4 = lerp(bumpColor4,bumpColor5,0.5f);
		bumpColor4 = lerp(bumpColor4,bumpColor6,0.5f);
		input.uvsets.xy = orguv;

		float normalInfluenceStrength = 3.0;
        float upDotNormal = saturate(dot(bumpColor4*2, float3(0, 1, 0)));

        foamIntensity *= lerp(0.0, 1.0, upDotNormal * normalInfluenceStrength);
        baseColorMap.rgb = lerp(baseColorMap.rgb, foamColor, foamIntensity);
	}

	//PE: Looks better but needed ?
	//float2 size;
	//float mipLevels;
	//texture_refraction.GetDimensions(0, size.x, size.y, mipLevels);
	//float4 refractiveColor = texture_refraction.SampleLevel(sampler_linear_clamp, ScreenCoord.xy+dudv3.rg, 0); //surface.roughness * mipLevels);
	//baseColorMap.rgb = lerp(refractiveColor.rgb,baseColorMap.rgb,input.color.a);
	//input.color.a = 1;

	baseColorMap.rgb = DEGAMMA(baseColorMap.rgb);
	color *= baseColorMap;

#endif // RIVERWATER
#endif

#ifndef GLASSOBJECT
#ifndef WATEROBJECT
#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
#ifdef ROCKTRIPLANAR
	float3 rockNormal;
	float4 rockSurfaceMap;
	color.rgb *= GGRockTriplanar(input.pos3D, normalize(input.nor), rockNormal, rockSurfaceMap);
#else
	[branch]
	if (GetMaterial().uvset_baseColorMap >= 0 && (g_xFrame_Options & OPTION_BIT_DISABLE_ALBEDO_MAPS) == 0)
	{
		const float2 UV_baseColorMap = GetMaterial().uvset_baseColorMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		float4 baseColorMap = texture_basecolormap.Sample(sampler_objectshader, UV_baseColorMap);
		baseColorMap.rgb = DEGAMMA(baseColorMap.rgb);
		color *= baseColorMap;
	}
#endif // ROCKTRIPLANAR
#endif
#endif // OBJECTSHADER_USE_UVSETS
#endif // WATEROBJECT
#endif // GLASSOBJECT


#ifdef GLASSOBJECT
	[branch]
	float4 baseColorMap = 0;
	if (GetMaterial().uvset_baseColorMap >= 0 && (g_xFrame_Options & OPTION_BIT_DISABLE_ALBEDO_MAPS) == 0)
	{
		const float2 UV_baseColorMap = GetMaterial().uvset_baseColorMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		baseColorMap = texture_basecolormap.Sample(sampler_objectshader, UV_baseColorMap);
	}

	NormalMapping(input.uvsets, surface.N, TBN, bumpColor);

	float transmission = 1 - clamp(0.5 * GetMaterial().customShaderParam1,0,1);
	float refraction = clamp(0.1 * GetMaterial().customShaderParam2,0,1);
	float brighten = GetMaterial().customShaderParam3;

	float2 size;
	float mipLevels;
	texture_refraction.GetDimensions(0, size.x, size.y, mipLevels);
	const float2 normal2D = mul((float3x3)g_xCamera_View, surface.N.xyz).xy;
	float4 refractiveColor = texture_refraction.SampleLevel(sampler_linear_clamp, ScreenCoord.xy + normal2D * refraction, 0);//surface.roughness * mipLevels); //surface.roughness * mipLevels);
	baseColorMap.rgb = lerp(refractiveColor.rgb * brighten,baseColorMap.rgb,transmission);

	baseColorMap.rgb = DEGAMMA(baseColorMap.rgb);
	color *= baseColorMap;
#endif


#ifdef OBJECTSHADER_USE_COLOR
	color *= input.color;
#endif // OBJECTSHADER_USE_COLOR


#ifndef DISABLE_ALPHATEST
	float alphatest = GetMaterial().alphaTest;
#ifndef TRANSPARENT
#ifndef ENVMAPRENDERING
	if (g_xFrame_Options & OPTION_BIT_TEMPORALAA_ENABLED)
	{
		alphatest = clamp(blue_noise(pixel, lineardepth).r, 0, 0.99);
	}
#endif // ENVMAPRENDERING
#endif // TRANSPARENT
#ifndef WEAPON_SHADOW
	clip(color.a - alphatest);
#endif
#endif // DISABLE_ALPHATEST

#ifndef GLASSOBJECT
#ifndef WATEROBJECT
#ifndef WATER
#ifdef OBJECTSHADER_USE_TANGENT
#ifndef OBJECTLOD
#ifdef ROCKTRIPLANAR
	surface.N = rockNormal;
	bumpColor = 0;
#else
	NormalMapping(input.uvsets, surface.N, TBN, bumpColor);
#endif // ROCKTRIPLANAR
#endif
#endif // OBJECTSHADER_USE_TANGENT
#endif // WATER
#endif // WATEROBJECT
#endif // GLASSOBJECT

	float4 surfaceMap = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
#ifdef ROCKTRIPLANAR
	surfaceMap = rockSurfaceMap;
#else
	[branch]
	if (GetMaterial().uvset_surfaceMap >= 0)
	{
		const float2 UV_surfaceMap = GetMaterial().uvset_surfaceMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		surfaceMap = texture_surfacemap.Sample(sampler_objectshader, UV_surfaceMap);
	}
#endif // ROCKTRIPLANAR
#endif
#endif // OBJECTSHADER_USE_UVSETS

#ifdef OBJECTLOD
	surfaceMap = float4(1,1,0,1);
#endif

	float4 specularMap = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
	[branch]
	if (GetMaterial().uvset_specularMap >= 0)
	{
		const float2 UV_specularMap = GetMaterial().uvset_specularMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		specularMap = texture_specularmap.Sample(sampler_objectshader, UV_specularMap);
		specularMap.rgb = DEGAMMA(specularMap.rgb);
	}
#endif
#endif // OBJECTSHADER_USE_UVSETS

	surface.create(GetMaterial(), color, surfaceMap, specularMap);

	// Emissive map:
	surface.emissiveColor = GetMaterial().emissiveColor;
#ifdef OBJECTLOD
	surface.emissiveColor.a = 0;
#endif

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
	[branch]
	if (surface.emissiveColor.a > 0 && GetMaterial().uvset_emissiveMap >= 0)
	{
		const float2 UV_emissiveMap = GetMaterial().uvset_emissiveMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		float4 emissiveMap = texture_emissivemap.Sample(sampler_objectshader, UV_emissiveMap);
		emissiveMap.rgb = DEGAMMA(emissiveMap.rgb);
		surface.emissiveColor *= emissiveMap;
	}
#endif
#endif // OBJECTSHADER_USE_UVSETS



#ifdef TERRAIN
	surface.N = 0;
	surface.albedo = 0;
	surface.f0 = 0;
	surface.roughness = 0;
	surface.occlusion = 0;
	surface.emissiveColor = 0;

	float4 sam;
	float4 blend_weights = input.color;
	blend_weights /= blend_weights.x + blend_weights.y + blend_weights.z + blend_weights.w;
	float3 baseN = normalize(input.nor);

	[branch]
	if (blend_weights.x > 0)
	{
		float4 color2 = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial().uvset_baseColorMap >= 0 && (g_xFrame_Options & OPTION_BIT_DISABLE_ALBEDO_MAPS) == 0)
		{
			float2 uv = GetMaterial().uvset_baseColorMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			color2 = texture_basecolormap.Sample(sampler_objectshader, uv);
			color2.rgb = DEGAMMA(color2.rgb);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		float4 surfaceMap2 = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial().uvset_surfaceMap >= 0)
		{
			float2 uv = GetMaterial().uvset_surfaceMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			surfaceMap2 = texture_surfacemap.Sample(sampler_objectshader, uv);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		Surface surface2;
		surface2.N = baseN;
		surface2.create(GetMaterial(), color2, surfaceMap2);

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial().normalMapStrength > 0 && GetMaterial().uvset_normalMap >= 0)
		{
			float2 uv = GetMaterial().uvset_normalMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			sam.rgb = texture_normalmap.Sample(sampler_objectshader, uv).rgb;
			sam.rgb = sam.rgb * 2 - 1;
			surface2.N = lerp(baseN, mul(sam.rgb, TBN), GetMaterial().normalMapStrength);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		surface2.emissiveColor = GetMaterial().emissiveColor;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial().uvset_emissiveMap >= 0 && any(GetMaterial().emissiveColor))
		{
			float2 uv = GetMaterial().uvset_emissiveMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			sam = texture_emissivemap.Sample(sampler_objectshader, uv);
			sam.rgb = DEGAMMA(sam.rgb);
			surface2.emissiveColor *= sam;
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		surface.N += surface2.N * blend_weights.x;
		surface.albedo += surface2.albedo * blend_weights.x;
		surface.f0 += surface2.f0 * blend_weights.x;
		surface.roughness += surface2.roughness * blend_weights.x;
		surface.occlusion += surface2.occlusion * blend_weights.x;
		surface.emissiveColor += surface2.emissiveColor * blend_weights.x;
	}

	[branch]
	if (blend_weights.y > 0)
	{
		float4 color2 = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial1().uvset_baseColorMap >= 0 && (g_xFrame_Options & OPTION_BIT_DISABLE_ALBEDO_MAPS) == 0)
		{
			float2 uv = GetMaterial1().uvset_baseColorMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			color2 = texture_blend1_basecolormap.Sample(sampler_objectshader, uv);
			color2.rgb = DEGAMMA(color2.rgb);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		float4 surfaceMap2 = 1;

#ifdef OBJECTSHADER_USE_UVSETS
		[branch]
		if (GetMaterial1().uvset_surfaceMap >= 0)
		{
			float2 uv = GetMaterial1().uvset_surfaceMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			surfaceMap2 = texture_blend1_surfacemap.Sample(sampler_objectshader, uv);
		}
#endif // OBJECTSHADER_USE_UVSETS

		Surface surface2;
		surface2.N = baseN;
		surface2.create(GetMaterial1(), color2, surfaceMap2);

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial1().normalMapStrength > 0 && GetMaterial1().uvset_normalMap >= 0)
		{
			float2 uv = GetMaterial1().uvset_normalMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			sam.rgb = texture_blend1_normalmap.Sample(sampler_objectshader, uv).rgb;
			sam.rgb = sam.rgb * 2 - 1;
			surface2.N = lerp(baseN, mul(sam.rgb, TBN), GetMaterial1().normalMapStrength);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		surface2.emissiveColor = GetMaterial1().emissiveColor;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial1().uvset_emissiveMap >= 0 && any(GetMaterial().emissiveColor))
		{
			float2 uv = GetMaterial1().uvset_emissiveMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			sam = texture_blend1_emissivemap.Sample(sampler_objectshader, uv);
			sam.rgb = DEGAMMA(sam.rgb);
			surface2.emissiveColor *= sam;
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		surface.N += surface2.N * blend_weights.y;
		surface.albedo += surface2.albedo * blend_weights.y;
		surface.f0 += surface2.f0 * blend_weights.y;
		surface.roughness += surface2.roughness * blend_weights.y;
		surface.occlusion += surface2.occlusion * blend_weights.y;
		surface.emissiveColor += surface2.emissiveColor * blend_weights.y;
	}

	[branch]
	if (blend_weights.z > 0)
	{
		float4 color2 = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial2().uvset_baseColorMap >= 0 && (g_xFrame_Options & OPTION_BIT_DISABLE_ALBEDO_MAPS) == 0)
		{
			float2 uv = GetMaterial2().uvset_baseColorMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			color2 = texture_blend2_basecolormap.Sample(sampler_objectshader, uv);
			color2.rgb = DEGAMMA(color2.rgb);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		float4 surfaceMap2 = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial2().uvset_surfaceMap >= 0)
		{
			float2 uv = GetMaterial2().uvset_surfaceMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			surfaceMap2 = texture_blend2_surfacemap.Sample(sampler_objectshader, uv);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		Surface surface2;
		surface2.N = baseN;
		surface2.create(GetMaterial2(), color2, surfaceMap2);

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial2().normalMapStrength > 0 && GetMaterial2().uvset_normalMap >= 0)
		{
			float2 uv = GetMaterial2().uvset_normalMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			sam.rgb = texture_blend2_normalmap.Sample(sampler_objectshader, uv).rgb;
			sam.rgb = sam.rgb * 2 - 1;
			surface2.N = lerp(baseN, mul(sam.rgb, TBN), GetMaterial2().normalMapStrength);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		surface2.emissiveColor = GetMaterial2().emissiveColor;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial2().uvset_emissiveMap >= 0 && any(GetMaterial2().emissiveColor))
		{
			float2 uv = GetMaterial2().uvset_emissiveMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			sam = texture_blend2_emissivemap.Sample(sampler_objectshader, uv);
			sam.rgb = DEGAMMA(sam.rgb);
			surface2.emissiveColor *= sam;
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		surface.N += surface2.N * blend_weights.z;
		surface.albedo += surface2.albedo * blend_weights.z;
		surface.f0 += surface2.f0 * blend_weights.z;
		surface.roughness += surface2.roughness * blend_weights.z;
		surface.occlusion += surface2.occlusion * blend_weights.z;
		surface.emissiveColor += surface2.emissiveColor * blend_weights.z;
	}

	[branch]
	if (blend_weights.w > 0)
	{
		float4 color2 = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial3().uvset_baseColorMap >= 0 && (g_xFrame_Options & OPTION_BIT_DISABLE_ALBEDO_MAPS) == 0)
		{
			float2 uv = GetMaterial3().uvset_baseColorMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			color2 = texture_blend3_basecolormap.Sample(sampler_objectshader, uv);
			color2.rgb = DEGAMMA(color2.rgb);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		float4 surfaceMap2 = 1;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial3().uvset_surfaceMap >= 0)
		{
			float2 uv = GetMaterial3().uvset_surfaceMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			surfaceMap2 = texture_blend3_surfacemap.Sample(sampler_objectshader, uv);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		Surface surface2;
		surface2.N = baseN;
		surface2.create(GetMaterial3(), color2, surfaceMap2);

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial3().normalMapStrength > 0 && GetMaterial3().uvset_normalMap >= 0)
		{
			float2 uv = GetMaterial3().uvset_normalMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			sam.rgb = texture_blend3_normalmap.Sample(sampler_objectshader, uv).rgb;
			sam.rgb = sam.rgb * 2 - 1;
			surface2.N = lerp(baseN, mul(sam.rgb, TBN), GetMaterial3().normalMapStrength);
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		surface2.emissiveColor = GetMaterial3().emissiveColor;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial3().uvset_emissiveMap >= 0 && any(GetMaterial3().emissiveColor))
		{
			float2 uv = GetMaterial3().uvset_emissiveMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			sam = texture_blend3_emissivemap.Sample(sampler_objectshader, uv);
			sam.rgb = DEGAMMA(sam.rgb);
			surface2.emissiveColor *= sam;
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		surface.N += surface2.N * blend_weights.w;
		surface.albedo += surface2.albedo * blend_weights.w;
		surface.f0 += surface2.f0 * blend_weights.w;
		surface.roughness += surface2.roughness * blend_weights.w;
		surface.occlusion += surface2.occlusion * blend_weights.w;
		surface.emissiveColor += surface2.emissiveColor * blend_weights.w;
	}

	surface.N = normalize(surface.N);

#endif // TERRAIN



#ifdef OBJECTSHADER_USE_EMISSIVE
	surface.emissiveColor *= unpack_rgba(input.emissiveColor);
#endif // OBJECTSHADER_USE_EMISSIVE


#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
	// Secondary occlusion map:
	[branch]
	if (GetMaterial().IsOcclusionEnabled_Secondary() && GetMaterial().uvset_occlusionMap >= 0)
	{
		const float2 UV_occlusionMap = GetMaterial().uvset_occlusionMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		surface.occlusion *= texture_occlusionmap.Sample(sampler_objectshader, UV_occlusionMap).r;
	}
#endif
#endif // OBJECTSHADER_USE_UVSETS


#ifndef PREPASS
#ifndef ENVMAPRENDERING
#ifndef TRANSPARENT
	surface.occlusion *= texture_ao.SampleLevel(sampler_linear_clamp, ScreenCoord, 0).r;
#endif // TRANSPARENT
#endif // ENVMAPRENDERING
#endif // PREPASS


#ifdef BRDF_ANISOTROPIC
	surface.anisotropy = GetMaterial().parallaxOcclusionMapping;
	surface.T = tangent.xyz;
	surface.B = normalize(cross(tangent.xyz, surface.N) * tangent.w); // Compute bitangent again after normal mapping
#endif // BRDF_ANISOTROPIC


#ifdef BRDF_SHEEN
	surface.sheen.color = GetMaterial().sheenColor.rgb;
	surface.sheen.roughness = GetMaterial().sheenRoughness;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
	[branch]
	if (GetMaterial().uvset_sheenColorMap >= 0)
	{
		const float2 UV_sheenColorMap = GetMaterial().uvset_sheenColorMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		surface.sheen.color *= DEGAMMA(texture_sheencolormap.Sample(sampler_objectshader, UV_sheenColorMap).rgb);
	}
	[branch]
	if (GetMaterial().uvset_sheenRoughnessMap >= 0)
	{
		const float2 uvset_sheenRoughnessMap = GetMaterial().uvset_sheenRoughnessMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		surface.sheen.roughness *= texture_sheenroughnessmap.Sample(sampler_objectshader, uvset_sheenRoughnessMap).a;
	}
#endif
#endif // OBJECTSHADER_USE_UVSETS
#endif // BRDF_SHEEN


#ifdef BRDF_CLEARCOAT
	surface.clearcoat.factor = GetMaterial().clearcoat;
	surface.clearcoat.roughness = GetMaterial().clearcoatRoughness;
	surface.clearcoat.N = input.nor;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
	[branch]
	if (GetMaterial().uvset_clearcoatMap >= 0)
	{
		const float2 UV_clearcoatMap = GetMaterial().uvset_clearcoatMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		surface.clearcoat.factor *= texture_clearcoatmap.Sample(sampler_objectshader, UV_clearcoatMap).r;
	}
	[branch]
	if (GetMaterial().uvset_clearcoatRoughnessMap >= 0)
	{
		const float2 uvset_clearcoatRoughnessMap = GetMaterial().uvset_clearcoatRoughnessMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		surface.clearcoat.roughness *= texture_clearcoatroughnessmap.Sample(sampler_objectshader, uvset_clearcoatRoughnessMap).g;
	}
#endif
#ifdef OBJECTSHADER_USE_TANGENT
#ifndef OBJECTLOD
	[branch]
	if (GetMaterial().uvset_clearcoatNormalMap >= 0)
	{
		const float2 uvset_clearcoatNormalMap = GetMaterial().uvset_clearcoatNormalMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		float3 clearcoatNormalMap = texture_clearcoatnormalmap.Sample(sampler_objectshader, uvset_clearcoatNormalMap).rgb;
		clearcoatNormalMap.b = clearcoatNormalMap.b == 0 ? 1 : clearcoatNormalMap.b; // fix for missing blue channel
		clearcoatNormalMap = clearcoatNormalMap * 2 - 1;
		surface.clearcoat.N = mul(clearcoatNormalMap, TBN);
	}
#endif
#endif // OBJECTSHADER_USE_TANGENT

	surface.clearcoat.N = normalize(surface.clearcoat.N);

#endif // OBJECTSHADER_USE_UVSETS
#endif // BRDF_CLEARCOAT


	surface.sss = GetMaterial().subsurfaceScattering;
	surface.sss_inv = GetMaterial().subsurfaceScattering_inv;

	surface.pixel = pixel;
	surface.screenUV = ScreenCoord;

	surface.layerMask = GetMaterial().layerMask;

	surface.update();

	float3 ambient = GetAmbient(surface.N);
	ambient = lerp(ambient, ambient * surface.sss.rgb, saturate(surface.sss.a));

	Lighting lighting;
	lighting.create(0, 0, ambient, 0);



#ifdef WATER

	//NORMALMAP
	float2 bumpColor0 = 0;
	float2 bumpColor1 = 0;
	//float2 bumpColor2 = 0; //REMOVE_WATER_RIPPLE
	[branch]
	if (GetMaterial().uvset_normalMap >= 0)
	{
		const float2 UV_normalMap = GetMaterial().uvset_normalMap == 0 ? input.uvsets.xy : input.uvsets.zw;
		bumpColor0 = 2 * texture_normalmap.Sample(sampler_objectshader, UV_normalMap - GetMaterial().texMulAdd.ww).rg - 1;
		bumpColor1 = 2 * texture_normalmap.Sample(sampler_objectshader, UV_normalMap + GetMaterial().texMulAdd.zw).rg - 1;
	}
	//bumpColor2 = texture_waterriples.SampleLevel(sampler_objectshader, ScreenCoord, 0).rg; // REMOVE_WATER_RIPPLE
	//bumpColor = float3(bumpColor0 + bumpColor1 + bumpColor2, 1)  * GetMaterial().refraction; // REMOVE_WATER_RIPPLE
	bumpColor = float3(bumpColor0 + bumpColor1 , 1)  * GetMaterial().refraction;
	surface.N = normalize(lerp(surface.N, mul(normalize(bumpColor), TBN), GetMaterial().normalMapStrength));
	bumpColor *= GetMaterial().normalMapStrength;

	//REFLECTION
	float4 reflectionUV = mul(g_xCamera_ReflVP, float4(surface.P, 1));
	reflectionUV.xy /= reflectionUV.w;
	reflectionUV.xy = reflectionUV.xy * float2(0.5, -0.5) + 0.5;
	lighting.indirect.specular += texture_reflection.SampleLevel(sampler_linear_mirror, reflectionUV.xy + bumpColor.rg, 0).rgb * surface.F;
#endif // WATER

#ifdef TRANSPARENT
	[branch]
	if (GetMaterial().transmission > 0)
	{
		surface.transmission = GetMaterial().transmission;

#ifdef OBJECTSHADER_USE_UVSETS
#ifndef OBJECTLOD
		[branch]
		if (GetMaterial().uvset_transmissionMap >= 0)
		{
			const float2 UV_transmissionMap = GetMaterial().uvset_transmissionMap == 0 ? input.uvsets.xy : input.uvsets.zw;
			float transmissionMap = texture_transmissionmap.Sample(sampler_objectshader, UV_transmissionMap).r;
			surface.transmission *= transmissionMap;
		}
#endif
#endif // OBJECTSHADER_USE_UVSETS

		float2 size;
		float mipLevels;
		texture_refraction.GetDimensions(0, size.x, size.y, mipLevels);
		const float2 normal2D = mul((float3x3)g_xCamera_View, surface.N.xyz).xy;
		float2 perturbatedRefrTexCoords = ScreenCoord.xy + normal2D * GetMaterial().refraction;
		float4 refractiveColor = texture_refraction.SampleLevel(sampler_linear_clamp, perturbatedRefrTexCoords, surface.roughness * mipLevels);
		surface.refraction.rgb = surface.albedo * refractiveColor.rgb;
		surface.refraction.a = surface.transmission;
	}
#endif // TRANSPARENT

#ifdef OBJECTSHADER_USE_ATLAS
	LightMapping(input.atl, lighting);
#endif // OBJECTSHADER_USE_ATLAS


#ifdef OBJECTSHADER_USE_EMISSIVE
	ApplyEmissive(surface, lighting);
#endif // OBJECTSHADER_USE_EMISSIVE


#ifdef PLANARREFLECTION
	lighting.indirect.specular += PlanarReflection(surface, bumpColor.rg) * surface.F;
#endif


#ifdef FORWARD
	ForwardLighting(surface, lighting);
#endif // FORWARD


#ifdef TILEDFORWARD
	float3 envAmbient = 0;
	TiledLighting(surface, lighting, envAmbient);
	lighting.indirect.diffuse += envAmbient;
#endif // TILEDFORWARD


#ifndef WATER
#ifndef ENVMAPRENDERING
#ifndef TRANSPARENT
	float4 ssr = texture_ssr.SampleLevel(sampler_linear_clamp, ScreenCoord, 0);
	lighting.indirect.specular = lerp(lighting.indirect.specular, ssr.rgb * surface.F, ssr.a);
#endif // TRANSPARENT
#endif // ENVMAPRENDERING
#endif // WATER


#ifdef WATER
	// WATER REFRACTION
	float sampled_lineardepth = texture_lineardepth.SampleLevel(sampler_point_clamp, ScreenCoord.xy + bumpColor.rg, 0) * g_xCamera_ZFarP;
	float depth_difference = sampled_lineardepth - lineardepth;
	surface.refraction.rgb = texture_refraction.SampleLevel(sampler_linear_mirror, ScreenCoord.xy + bumpColor.rg * saturate(0.5 * depth_difference), 0).rgb;
	if (depth_difference < 0)
	{
		// Fix cutoff by taking unperturbed depth diff to fill the holes with fog:
		sampled_lineardepth = texture_lineardepth.SampleLevel(sampler_point_clamp, ScreenCoord.xy, 0) * g_xCamera_ZFarP;
		depth_difference = sampled_lineardepth - lineardepth;
	}
	// WATER FOG:
	surface.refraction.a = 1 - saturate(color.a * 0.1 * depth_difference);
	color.a = 1;

#endif // WATER

#ifdef RIVERWATER
	// GG: as the ocean's surface (oceanSurfacePS): the scene beneath seen through the water, fading to the water colour
	// with the depth looked through, from the fog start to the fog max (customShaderParam5 and 7, in units) and never
	// clearer than 1 - fog minimum (the base colour's alpha); drawn opaque over it
	{
		const float2 perturb = surface.N.xz * 0.04;
		float riverSampledDepth = texture_lineardepth.SampleLevel(sampler_linear_clamp, ScreenCoord + perturb, 0) * g_xCamera_ZFarP;
		float riverDepth = max(0, (riverSampledDepth - lineardepth) * 0.0254);
		const float2 refractUV = ScreenCoord + perturb * saturate(0.5 * riverDepth);
		const float3 refracted = texture_refraction.SampleLevel(sampler_linear_mirror, refractUV, 0).rgb;
		riverSampledDepth = texture_lineardepth.SampleLevel(sampler_linear_clamp, refractUV, 0) * g_xCamera_ZFarP;
		riverDepth = max(0, (riverSampledDepth - lineardepth) * 0.0254);
		// the path through the water is longer the more slanting the view
		riverDepth += riverDepth / max(0.001, dist * 0.0254) * max(0, g_xCamera_CamPos.y - surface.P.y) * 0.0254;
		const float fogMin = GetMaterial().customShaderParam5 * 0.0254;
		const float fogMax = max(fogMin + 0.01, GetMaterial().customShaderParam7 * 0.0254);
		float fade = exp(max(0, riverDepth - fogMin) * (-4.0 / (fogMax - fogMin)));
		fade = min(saturate(1 - GetMaterial().baseColor.a), fade);
		// foam (the banks', and white water where the river is turbulent) hides the water beneath
		surface.albedo = lerp(surface.albedo, 0.8, riverFoam);
		surface.refraction = float4(refracted, fade * (1 - riverFoam));
		color.a = 1;
	}
#endif // RIVERWATER

#ifdef UNLIT
	lighting.direct.diffuse = 1;
	lighting.indirect.diffuse = 0;
	lighting.direct.specular = 0;
	lighting.indirect.specular = 0;
#endif // UNLIT


	ApplyLighting(surface, lighting, color);


#ifdef OBJECTSHADER_USE_POSITION3D
	ApplyFog(dist, g_xCamera_CamPos, surface.V, color);
#endif // OBJECTSHADER_USE_POSITION3D

	// leelee - experiment - just show emissive
	//lighting.direct.specular = float3(0, 0, 0);
	//ApplyEmissive(surface, lighting);
	//color.xyz = float3(121.0f/255.0f, 255.0f / 255.0f, 255.0f / 255.0f);// lighting.direct.specular.xyz;

	color = max(0, color);

	// end point:
#ifdef PREPASS
	OutputPrepass output;
	output.velocity = float4(velocity, 0, 0); /*FORMAT_R16G16_FLOAT*/
	output.readback = 0;
	return output;
#else
#ifdef OUTPUT_GBUFFER
	//#ifdef OBJECTSHADER_USE_TANGENT
	// lovely hacks to visualize pixel shader data :) 
	//color = float4(input.uvsets.x,input.uvsets.y, 0, 1);
	//color = float4(surface.N.x,surface.N.y,surface.N.z, 1);
	//float3 V = float3(0, 1, 0);
	//V = mul(TBN, V);
	//color = float4(V.x,V.y,V.z, 1);
	//color = float4(input.tan.x,input.tan.y,input.tan.z, 1);
	//#endif
	return CreateGBuffer(color, surface);
#else
	return color;
#endif // OUTPUT_GBUFFER
#endif // PREPASS

}


#endif // OBJECTSHADER_COMPILE_PS



#endif // WI_OBJECTSHADER_HF

