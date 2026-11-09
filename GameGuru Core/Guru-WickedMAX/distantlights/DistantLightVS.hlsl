// GG: distant lights (DistantLightManager): each instance a light, drawn as a camera-facing dot whose size on screen is
// its own world size there, kept between the smallest and largest dot; faded in past a distance, dimmed by the scene's fog
// as a surface there is (never cut off), and by the global amount; drawn a little toward the camera so its own lamp's mesh
// doesn't hide it, and at the far plane when further than it

struct DistantLight
{
	float4 posSize; // xyz the light, w its size (world units)
	float4 color; // rgb its colour by its brightness, a the share of the fog it takes (0 to 1)
};

StructuredBuffer<DistantLight> lights : register(t1);

cbuffer DistantLightCB : register(b2)
{
	matrix g_DLViewProj;
	float4 g_DLCamPos; // xyz the camera, w the projection's vertical scale
	float4 g_DLScreen; // x, y the target's size in pixels, z the smallest dot, w the largest (pixels across its middle)
	float4 g_DLFade; // x faded in from, y full by (world units), z the global amount, w how far drawn toward the camera
	float4 g_DLFog; // x where the fog starts, y how fast it thickens past there (4 / (full - start)), z the share of it a light takes
	float4 g_DLWater; // x the water's height (none: far below), y where its fog starts, z how fast it thickens, w the most it leaves
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
	// what the scene's fog (objectHF.hlsli ApplyFog) leaves of a surface here without the realistic sky: under water, the
	// camera and the light both below its surface, the water's fog over the distance and the light's depth
	float keep;
	[branch] if (l.posSize.y < g_DLWater.x && g_DLCamPos.y < g_DLWater.x)
	{
		keep = min(g_DLWater.w, exp(-max(0.0f, distance + g_DLWater.x - l.posSize.y - g_DLWater.y) * g_DLWater.z));
	}
	else
	{
		keep = 1.0f - l.color.a * g_DLFog.z * (1.0f - exp(-max(0.0f, distance - g_DLFog.x) * g_DLFog.y));
	}

	VSOut output;
	output.pos = clip;
	output.corner = corner;
	output.color = l.color.rgb * g_DLFade.z * fadeIn * keep;
	// further than the far plane: drawn at it (the depth is reversed, far is 0), where only the scene drawn nearer hides it
	output.pos.z = max(output.pos.z, 0.0f);
	// none to draw: behind the far plane by its depth alone (all its corners alike)
	if (fadeIn <= 0.0f || max(output.color.r, max(output.color.g, output.color.b)) < 0.0001f) output.pos.z = -output.pos.w;
	return output;
}
