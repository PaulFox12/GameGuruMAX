Texture2DArray texTree : register( t50 );
Texture2D<float> texNoise : register( t51 );

SamplerState samplerBilinearWrap : register( s0 );
SamplerState samplerTrilinearClamp : register( s1 );

#include "PBR/ShaderInterop_Renderer.h"
#include "GGTreesConstants.hlsli"

struct PixelIn
{
    float4 position : SV_POSITION;
	float3 worldPos : TEXCOORD0;
	uint data : TEXCOORD2;
	float2 uv : TEXCOORD1;
	nointerpolation float lodFade : TEXCOORD5;
};

float4 main( PixelIn IN ) : SV_TARGET
{
	uint treeType = GetTreeType( IN.data );
	uint index = GetTreeVariation( IN.data );

	if ( TreeDither( IN.position.xy ) >= IN.lodFade ) discard;

	float alpha = texTree.Sample( samplerTrilinearClamp, float3(IN.uv, treeType) ).a;
	if ( alpha < 0.3 ) discard;

	return float4( 1, 1, 1, 1 );
}