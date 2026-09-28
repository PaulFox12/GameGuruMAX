#ifndef GG_DECALHF
#define GG_DECALHF

// GameGuru's projected decals, applied by the object shaders' decal loops (objectHF.hlsli) and the terrain's
// (GGTerrainVirtualPBR_PS.hlsl) through GGDecalColor. A decal without a GameGuru flag is Wicked's planar decal, unchanged.
// - ENTITY_FLAG_DECAL_FACING (wiRenderer.cpp, from DecalComponent::facing): paints only surfaces whose geometric normal
//   faces along the decal's Z by more than the cosine in its cone angle slot, fading in over the next 0.1, so the inside
//   face of a thin wall and the ground under an upright box stay clean

// the geometric normal of the surface being shaded (before normal mapping), set by the pixel shader before its lighting
static float3 decal_faceN = float3(0, 1, 0);

// the decal's colour at P, and in edgeBlend how much of it applies there (0 where it paints nothing), which also scales
// its emissive
inline float4 GGDecalColor(in ShaderEntity decal, in float3 P, in float3 P_dx, in float3 P_dy, out float edgeBlend)
{
	edgeBlend = 0;
	float4x4 decalProjection = MatrixArray[decal.GetMatrixIndex()];
	const float4 texMulAdd = decalProjection[3];
	decalProjection[3] = float4(0, 0, 0, 1);
	const float3 clipSpacePos = mul(decalProjection, float4(P, 1)).xyz;
	const float3 uvw = clipSpacePos.xyz * float3(0.5, -0.5, 0.5) + 0.5;
	[branch]
	if (!is_saturated(uvw))
		return 0;

	float facingBlend = 1;
	[branch]
	if (decal.GetFlags() & ENTITY_FLAG_DECAL_FACING)
	{
		// the decal's Z in world space is the gradient of its box's z
		const float facing = dot(decal_faceN, normalize(decalProjection[2].xyz));
		facingBlend = saturate((facing - decal.GetConeAngleCos()) * 10);
		[branch]
		if (facingBlend <= 0)
			return 0;
	}

	// mipmapping needs to be performed by hand:
	const float2 decalDX = mul(P_dx, (float3x3)decalProjection).xy * texMulAdd.xy;
	const float2 decalDY = mul(P_dy, (float3x3)decalProjection).xy * texMulAdd.xy;
	float4 decalColor = texture_decalatlas.SampleGrad(sampler_linear_clamp, uvw.xy * texMulAdd.xy + texMulAdd.zw, decalDX, decalDY);
	// blend out if close to cube Z:
	edgeBlend = (1 - pow(saturate(abs(clipSpacePos.z)), 8)) * facingBlend;
	decalColor.a *= edgeBlend;
	decalColor *= decal.GetColor();
	return decalColor;
}

#endif // GG_DECALHF
