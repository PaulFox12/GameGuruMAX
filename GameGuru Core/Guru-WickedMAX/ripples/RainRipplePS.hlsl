// GG: rain ripples drawn as one batch (RippleManager): the ripple image's frame, faded, and faded again into the fog
// (added to the scene, so fog takes it away rather than tinting it)

Texture2D rippleTexture : register(t0);
SamplerState rippleSampler : register(s0);

cbuffer RippleCB : register(b2)
{
	matrix g_RippleViewProj;
	float4 g_RippleCamPos;
	float4 g_RippleAnim;
	float4 g_RippleFog; // x where the fog starts, y where it is full
};

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float fade : TEXCOORD1;
	float3 world : TEXCOORD2;
};

float4 main(VSOut input) : SV_TARGET
{
	float4 color = rippleTexture.Sample(rippleSampler, input.uv);
	float fog = 0;
	if (g_RippleFog.y > g_RippleFog.x)
	{
		float distance = length(input.world - g_RippleCamPos.xyz);
		fog = saturate((distance - g_RippleFog.x) / (g_RippleFog.y - g_RippleFog.x));
	}
	color.a *= input.fade * (1.0f - fog);
	return color;
}
