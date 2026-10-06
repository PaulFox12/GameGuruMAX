
/*
cbuffer CameraCB : register( b1 )
{
	float4x4	g_xCamera_VP;			// View*Projection
	float4		g_xCamera_ClipPlane;
	float3		g_xCamera_CamPos;
};
*/

#include "PBR/globals.hlsli"

#include "GGGrassConstants.hlsli"

struct VertexIn
{
	float2 position : POSITION;
	float3 offset: OFFSET;
	uint data : DATA;
	uint instanceID : SV_InstanceID;
};

struct VertexOut
{
	float4 position : SV_POSITION;
	float3 worldPos : TEXCOORD0;
	uint data : TEXCOORD2;
	float4 origPos : TEXCOORD3;
	float2 uv : TEXCOORD1;
	float2 uvNoise : TEXCOORD4;
};

VertexOut main( VertexIn IN )
{
    VertexOut OUT;

	// GG: the card's position as the prepass works it out (GrassCardWorldPos, precise), so an exact depth test matches it
	float4 posOrig;
	precise float4 pos = GrassCardWorldPos( IN.position, IN.offset, IN.data, IN.instanceID, posOrig );
	/*
	float offset = (IN.instanceID & 0x7F) * 16;
	float dist = length( g_xCamera_CamPos - pos.xyz );
	dist = 1 - saturate( (dist - grass_lodDist + offset) / GGGRASS_LOD_TRANSITION );
	pos.y = (pos.y - IN.offset.y) * dist + IN.offset.y;

	posOrig.w = lerp( 1, IN.position.y, dist );
	*/
	posOrig.w = IN.position.y; // 0 at the root, 1 at the tip, for the pixel shader's shade up the blade (was set only in the block above)
	OUT.worldPos = pos.xyz;
	precise float4 clipPos = mul( g_xCamera_VP, pos );
	OUT.position = clipPos;
	OUT.origPos = posOrig;
	OUT.uv.x = IN.position.x + 0.5;
	OUT.uv.y = 1 - IN.position.y;
	OUT.uvNoise = OUT.uv + (IN.instanceID & 0xFF) / 256.0;
	OUT.data = IN.data;
		
    return OUT;
}

