Texture2D<float> texNoise : register( t51 );
Texture2DArray texBranchesHigh : register( t54 );

SamplerState samplerBilinearWrap : register( s0 );
SamplerState samplerTrilinearWrap : register( s2 );

#include "PBR/ShaderInterop_Renderer.h"
#include "GGTreesConstants.hlsli"

struct PixelIn
{
    float4 position : SV_POSITION;
	float3 worldPos : TEXCOORD0;
	float2 uv : TEXCOORD1;
	uint data : TEXCOORD2;
	nointerpolation float lodFade : TEXCOORD5;
};

float4 main( PixelIn IN ) : SV_TARGET
{
	uint treeType = GetTreeType( IN.data );
	uint index = GetTreeVariation( IN.data );

	if ( TreeDither( IN.position.xy ) < IN.lodFade ) discard;

	// GG: cascades 2 and on take the alpha one mip coarser, as each of their texels covers many leaves
	float alpha = texBranchesHigh.SampleBias( samplerTrilinearWrap, float3(IN.uv, treeType), tree_shadowFar ? 1.0 : 0.0 ).a;
	if ( alpha < 0.3 ) discard;

	return float4( 1, 1, 1, 1 );
}