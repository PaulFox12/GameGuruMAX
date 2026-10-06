Texture2DArray<float> texPageTableArray : register( t53 );
Texture2D<float> texPageTableFinal : register( t54 );
SamplerState sampler0 : register( s0 );

#include "GGTerrainConstants.hlsli"
#include "../GGTerrainPageSettings.h"
#include "GGTerrainPageRequestHF.hlsli"

struct PixelIn
{
    float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float2 uvPos : TEXCOORD1;
	uint lodLevel : TEXCOORD2;
};

struct Output
{
	float4 velocity : SV_TARGET0;
	uint   readback : SV_TARGET1;  // virtual texture read back
};

Output main( PixelIn IN )
{
	Output output;
	output.velocity = float4( 0, 0, 0, 0 );

	output.readback = GGTerrainPageRequest( IN.uv, IN.uvPos, IN.lodLevel, (uint2) IN.position.xy, sampler0 );
	
	return output;
}