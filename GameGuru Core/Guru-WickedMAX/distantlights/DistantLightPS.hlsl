// GG: distant lights (DistantLightManager): a round dot, brightest in its middle, added to the scene

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 corner : TEXCOORD0;
	float3 color : TEXCOORD1;
};

float4 main(VSOut input) : SV_TARGET
{
	float edge = saturate(1.0f - length(input.corner));
	return float4(input.color * edge * edge, 0.0f);
}
