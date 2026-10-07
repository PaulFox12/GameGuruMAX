// GG: rain ripples drawn as one batch (RippleManager): each instance a ring, its quad built here on its surface, its
// animation frame and its fade worked out from its age, as the water ripple decal played them

struct RippleInstance
{
	float4 posSizeX;
	float4 normalSizeY;
	float4 anim; // x its age in seconds, y its first frame, z the frame it ends before
};

StructuredBuffer<RippleInstance> ripples : register(t1);

cbuffer RippleCB : register(b2)
{
	matrix g_RippleViewProj;
	float4 g_RippleCamPos;
	float4 g_RippleAnim; // x frames a second, y frames across, z frames down
	float4 g_RippleFog; // x where the fog starts, y where it is full
};

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float fade : TEXCOORD1;
	float3 world : TEXCOORD2;
};

static const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(0, 1), float2(1, 0), float2(1, 1) };

VSOut main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	RippleInstance r = ripples[instanceID];
	float2 corner = corners[vertexID];

	// the quad on the ring's surface: flat ground gives x along world x and y along world z, as the decal lay
	float3 n = normalize(r.normalSizeY.xyz);
	float3 reference = abs(n.z) < 0.99f ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 axisX = normalize(cross(reference, n));
	float3 axisY = cross(n, axisX);
	float3 world = r.posSizeX.xyz + axisX * ((corner.x - 0.5f) * r.posSizeX.w) + axisY * ((corner.y - 0.5f) * r.normalSizeY.w);

	// the frame from its age, and the fade: in over the first half of its frames and out over the second
	float frame = r.anim.y + r.anim.x * g_RippleAnim.x;
	float frameEnd = r.anim.z;
	uint across = (uint)g_RippleAnim.y;
	uint down = (uint)g_RippleAnim.z;
	uint index = (uint)frame;
	float2 frameSize = float2(1.0f / across, 1.0f / down);
	float2 frameOrigin = float2(index % across, index / across) * frameSize;
	float remaining = saturate((frameEnd - frame) / frameEnd);
	float fade = remaining >= 0.5f ? 1.0f - (remaining - 0.5f) * 2.0f : remaining * 2.0f;

	VSOut output;
	output.pos = mul(float4(world, 1.0f), g_RippleViewProj);
	output.uv = frameOrigin + corner * frameSize;
	output.fade = fade;
	output.world = world;
	return output;
}
