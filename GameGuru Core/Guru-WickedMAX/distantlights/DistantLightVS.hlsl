// GG: distant lights (DistantLightManager): each instance a light, drawn as a camera-facing dot whose size on screen is
// its own world size there, kept between the smallest and largest dot; faded in past a distance, into the fog, and by the
// global amount; drawn a little toward the camera so its own lamp's mesh doesn't hide it

struct DistantLight
{
	float4 posSize; // xyz the light, w its size (world units)
	float4 color; // rgb its colour, a its brightness
};

StructuredBuffer<DistantLight> lights : register(t1);

cbuffer DistantLightCB : register(b2)
{
	matrix g_DLViewProj;
	float4 g_DLCamPos; // xyz the camera, w the projection's vertical scale
	float4 g_DLScreen; // x, y the target's size in pixels, z the smallest dot, w the largest (pixels across its middle)
	float4 g_DLFade; // x faded in from, y full by (world units), z the global amount, w how far drawn toward the camera
	float4 g_DLFog; // x where the fog starts, y where it is full
};

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 corner : TEXCOORD0;
	float3 color : TEXCOORD1;
};

static const float2 corners[6] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(-1, 1), float2(1, -1), float2(1, 1) };

VSOut main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	DistantLight l = lights[instanceID];
	float3 toCamera = g_DLCamPos.xyz - l.posSize.xyz;
	float distance = length(toCamera);
	float3 world = l.posSize.xyz + (distance > 0.001f ? toCamera / distance * min(g_DLFade.w, distance * 0.5f) : float3(0, 0, 0));
	float4 clip = mul(float4(world, 1.0f), g_DLViewProj);

	// its size on screen, between the smallest and largest dot (half a dot across each way from the middle)
	float pixels = l.posSize.w * g_DLCamPos.w / max(clip.w, 0.001f) * g_DLScreen.y * 0.5f;
	float drawn = clamp(pixels, g_DLScreen.z, g_DLScreen.w) * 0.5f;
	float2 corner = corners[vertexID];
	clip.xy += corner * drawn * 2.0f / g_DLScreen.xy * clip.w;

	float fadeIn = saturate((distance - g_DLFade.x) / max(g_DLFade.y - g_DLFade.x, 1.0f));
	float fog = g_DLFog.y > g_DLFog.x ? saturate((distance - g_DLFog.x) / (g_DLFog.y - g_DLFog.x)) : 0.0f;

	VSOut output;
	output.pos = clip;
	output.corner = corner;
	output.color = l.color.rgb * l.color.a * g_DLFade.z * fadeIn * (1.0f - fog);
	// none to draw: behind the far plane by its depth alone (all its corners alike)
	if (fadeIn <= 0.0f || fog >= 1.0f || g_DLFade.z <= 0.0f) output.pos.z = -output.pos.w;
	return output;
}
