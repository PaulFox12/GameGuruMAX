
#include "GGTerrainConstants.hlsli"

Texture2DArray<float4> texColor  : register( t50 );
Texture2DArray<float2> texNormal : register( t51 );

#ifdef GGTERRAIN_USE_SURFACE_TEXTURE
  Texture2DArray<float4> texSurface : register( t52 ); // R: occlusion, G: roughness, B: metalness
#else
  Texture2DArray<float2> texRoughnessMetalness : register( t52 );
  Texture2DArray<float>  texAO     : register( t53 );
#endif

Texture2DArray<float>  texLODHeight  : register( t54 );
Texture2DArray<float4> texLODNormals : register( t55 );

Texture2D<float>  texMask        : register( t56 );
Texture2D<float>  texMaterialMap : register( t57 );

Texture2DArray texGrass : register( t58 );

SamplerState samplerTriWrap : register( s0 );
SamplerState samplerBiClamp : register( s1 );
SamplerState samplerBiWrap  : register( s2 );
SamplerState samplerPointClamp : register( s3 );

#define ANGLE_STEP 50.069
#define ANGLE_SMALL_STEP 50.069 * 0.05

struct PixelIn
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float2 uv2 : TEXCOORD1;
	float2 uv3 : TEXCOORD2;
	float2 uvMat : TEXCOORD3;
	uint lodLevel : TEXCOORD4;
};

struct PixelOut
{
	float4 color : SV_TARGET0; // RGB: color, A: metalness
	float4 normal : SV_TARGET1; // RG: normal, B: roughness, A: ambient occulusion
};

float3 ExpandNormal( float2 normalRG )
{
	float3 normal;
	normal.rg = normalRG * 2 - 1;
	normal.b = 1 - (normal.r*normal.r + normal.g*normal.g);
	if ( normal.b > 0 ) normal.b = sqrt( normal.b );
	return normal;
}

struct SurfaceValues
{
	float3 color;
	float metalness;
	float3 normal;
	float roughness;
	float ao;
};

SurfaceValues SampleTexture( float2 uv, uint index, float2 uvDX, float2 uvDY )
{
	SurfaceValues output;

	float3 arrayUV = float3( uv, index );
	output.color = texColor.SampleGrad( samplerTriWrap, arrayUV, uvDX, uvDY ).rgb;
	
#ifdef GGTERRAIN_USE_SURFACE_TEXTURE
	float4 surface = texSurface.SampleGrad( samplerTriWrap, arrayUV, uvDX, uvDY );
	output.ao = surface.r;
	output.roughness = surface.g;
	output.metalness = surface.b;
#else
	output.ao = texAO.SampleGrad( samplerTriWrap, arrayUV, uvDX, uvDY );
	float2 roughnessMetalness = texRoughnessMetalness.SampleGrad( samplerTriWrap, arrayUV, uvDX, uvDY );
	output.roughness = roughnessMetalness.r;
	output.metalness = roughnessMetalness.g;
#endif

	output.normal = ExpandNormal( texNormal.SampleGrad( samplerTriWrap, arrayUV, uvDX, uvDY ).rg );

	return output;
}

SurfaceValues LerpSurface( SurfaceValues surface1, SurfaceValues surface2, float t )
{
	SurfaceValues output;

	output.color = lerp( surface1.color, surface2.color, t );
	output.metalness = lerp( surface1.metalness, surface2.metalness, t );
	output.roughness = lerp( surface1.roughness, surface2.roughness, t );
	output.ao = lerp( surface1.ao, surface2.ao, t );

	float3 normal = lerp( surface1.normal, surface2.normal, t );
	output.normal = normalize( normal );

	return output;
}

// the four surfaces added by weight (they sum to 1)
SurfaceValues WeightSurfaces( SurfaceValues surfaces[4], float4 weight )
{
	SurfaceValues output;

	output.color = surfaces[0].color * weight.x + surfaces[1].color * weight.y + surfaces[2].color * weight.z + surfaces[3].color * weight.w;
	output.metalness = dot( float4( surfaces[0].metalness, surfaces[1].metalness, surfaces[2].metalness, surfaces[3].metalness ), weight );
	output.roughness = dot( float4( surfaces[0].roughness, surfaces[1].roughness, surfaces[2].roughness, surfaces[3].roughness ), weight );
	output.ao = dot( float4( surfaces[0].ao, surfaces[1].ao, surfaces[2].ao, surfaces[3].ao ), weight );

	float3 normal = surfaces[0].normal * weight.x + surfaces[1].normal * weight.y + surfaces[2].normal * weight.z + surfaces[3].normal * weight.w;
	output.normal = normalize( normal );

	return output;
}

// value noise, -1 to 1, smooth at a scale of 1
float EdgeHash( float2 p )
{
	float3 p3 = frac( p.xyx * 0.1031 );
	p3 += dot( p3, p3.yzx + 33.33 );
	return frac( (p3.x + p3.y) * p3.z );
}

float EdgeNoise( float2 p )
{
	float2 cell = floor( p );
	float2 f = frac( p );
	f = f * f * (3 - 2 * f);
	float a = EdgeHash( cell );
	float b = EdgeHash( cell + float2(1, 0) );
	float c = EdgeHash( cell + float2(0, 1) );
	float d = EdgeHash( cell + float2(1, 1) );
	return lerp( lerp( a, b, f.x ), lerp( c, d, f.x ), f.y ) * 2 - 1;
}

SurfaceValues SampleTexture2( uint index, float2 uv, float4 uvDXY, uint mask[4], float2 interp )
{
	uint material = index & 0xFF;

	// special case for checkerboard texture to stop it being rotated
	if ( material == 31 ) return SampleTexture( uv, 31, uvDXY.xy, uvDXY.zw );

	uint offset = (index >> 8) ? 32 : 0;

	SurfaceValues output[ 4 ];

	for( uint i = 0; i < 4; i++ )
	{
		uint index2 = mask[i] + offset;
		float2x2 rotMat = { terrain_maskRotMat[ index2 ].x, terrain_maskRotMat[ index2 ].y, terrain_maskRotMat[ index2 ].z, terrain_maskRotMat[ index2 ].w };
		
		float2 rotatedUV = mul( rotMat, uv );
		float2 uvDX = mul( rotMat, uvDXY.xy );
		float2 uvDY = mul( rotMat, uvDXY.zw );

		output[ i ] = SampleTexture( rotatedUV, material, uvDX, uvDY );
		output[ i ].normal.xy = mul( output[i].normal.xy, rotMat );
	}

	output[2] = LerpSurface( output[3], output[2], interp.x );
	output[0] = LerpSurface( output[0], output[1], interp.x );

	output[0] = LerpSurface( output[2], output[0], interp.y );

	return output[0];
}
 
PixelOut main( PixelIn IN )
{
	PixelOut output;

	uint heightLevel = IN.lodLevel & 0xF;
	uint detailLevel = (IN.lodLevel >> 4) & 0xF;

	float height = texLODHeight.Sample( samplerBiClamp, float3(IN.uv2, heightLevel) );
	float3 normal = texLODNormals.Sample( samplerBiClamp, float3(IN.uv2, heightLevel) ).rgb;
	normal = normal * 2 - 1;
	normal = normalize( normal );

	/*
	output.color.rgb = normal;
	output.color.a = 0;
	output.normal = float4( 0, 0, 1, 1 );
	return output;
	*/

	// mask texture
	float2 interp;
    interp = frac( IN.uv3 * 1024 );
	interp = frac( interp + 0.5 + (2.0/1024.0) );
	interp = smoothstep( 0.3, 0.7, interp );
    
	float4 mask = texMask.GatherRed( samplerBiWrap, IN.uv3 );
	
	if ( terrain_flags & GGTERRAIN_SHADER_FLAG_SHOW_MASK )
	{
		mask *= 8;
		float c1 = lerp( mask.w, mask.z, interp.x );
		float c2 = lerp( mask.x, mask.y, interp.x );
		float color = lerp( c1, c2, interp.y );
	
		output.color.rgb = float3( color, color, color );
		output.color.a = 0;
		output.normal = float4( 0, 1, 1, 1 );
		return output;
	}
		
	uint maskArray[4];
	maskArray[0] = mask.x * 255;
	maskArray[1] = mask.y * 255;
	maskArray[2] = mask.z * 255;
	maskArray[3] = mask.w * 255;

	// user material map
	// edge breakup: the map read where a smooth noise moves it, by up to a third of its cell, so the edges painted along its
	// cells don't run straight; faded out on pages whose texels span a cell or more (the distance), which keep the old blend
	float2 uvMat = IN.uvMat;
	float2 matCell = IN.uvMat * 4096;
	float footprint = max( length( ddx( matCell ) ), length( ddy( matCell ) ) );
	float breakup = 0;
	if ( terrain_edgeBreakup > 0 )
	{
		breakup = terrain_edgeBreakup * saturate( 2 - footprint * 2 );
		float2 warp = float2( EdgeNoise( matCell * 0.7 ), EdgeNoise( matCell * 0.7 + 31.7 ) );
		uvMat += warp * (breakup * 0.35 / 4096);
	}

	float2 interp2;
    interp2 = frac( uvMat * 4096 );
	interp2 = frac( interp2 + 0.5 + (2.0/1024.0) );
    
	float4 materialMap = texMaterialMap.GatherRed( samplerBiClamp, uvMat );

	if ( terrain_flags & GGTERRAIN_SHADER_FLAG_SHOW_MAT_MAP )
	{
		materialMap *= 8;
		float c1 = lerp( materialMap.w, materialMap.z, interp2.x );
		float c2 = lerp( materialMap.x, materialMap.y, interp2.x );
		float color = lerp( c1, c2, interp2.y );
		//color = texMaterialMap.Sample( samplerBiClamp, IN.uvMat ).r*8;
	
		output.color.rgb = float3( color, color, color );
		output.color.a = 0;
		output.normal = float4( 0, 1, 1, 1 );
		return output;
	}

	uint matArray[4];
	matArray[0] = (materialMap.x * 255);
	matArray[1] = (materialMap.y * 255);
	matArray[2] = (materialMap.z * 255);
	matArray[3] = (materialMap.w * 255);

	// original derivatives
	float4 uvdxy;
	uvdxy.xy = ddx( IN.uv );
	uvdxy.zw = ddy( IN.uv );

	SurfaceValues finalSurface;

	if ( !all(materialMap) )
	{
		finalSurface = SampleTexture2( terrain_baseLayerMaterial, IN.uv, uvdxy, maskArray, interp );

		// blend layers
		int i;
		for( i = 0; i < 5; i++ )
		{
			float t = clamp( (height-terrain_layers[i].start)*terrain_layers[i].transition, 0.0, 1.0 );
			if ( t >= 1 ) finalSurface = SampleTexture2( terrain_layers[i].material, IN.uv, uvdxy, maskArray, interp );
			else if ( t > 0 )
			{
				SurfaceValues layer = SampleTexture2( terrain_layers[i].material, IN.uv, uvdxy, maskArray, interp );
				finalSurface = LerpSurface( finalSurface, layer, t );
			}
		}

		// normal Y used for slope blending
		float normaly = 1 - abs(normal.y);

		// blend slopes
		for( i = 0; i < 2; i++ )
		{
			float t = clamp((normaly-terrain_slopes[i].start)*terrain_slopes[i].transition, 0.0, 1.0);
			if ( t >= 1 ) finalSurface = SampleTexture2( terrain_slopes[i].material, IN.uv, uvdxy, maskArray, interp );
			else if ( t > 0 ) 
			{
				SurfaceValues slope = SampleTexture2( terrain_slopes[i].material, IN.uv, uvdxy, maskArray, interp );
				finalSurface = LerpSurface( finalSurface, slope, t );
			}
		}
	}
	
	// sample user painted materials
	SurfaceValues surfaces[4];
	for( uint i = 0; i < 4; i++ )
	{
		if ( matArray[i] == 0 ) surfaces[ i ] = finalSurface;
		else surfaces[ i ] = SampleTexture2( matArray[i]-1 | 0x100, IN.uv, uvdxy, maskArray, interp );
	}	

	// blend user painted materials with default height based materials calculated above
	if ( breakup > 0 )
	{
		// each material's share of the four cells (the corners' bilinear weights, summed where they hold the same material),
		// moved by a noise of that material's own and sharpened, so where two meet the edge follows the middle of the blend
		// (diagonal along a diagonal stroke, not a staircase of cells) unevenly, and the blend is narrower
		float4 weight = float4( (1 - interp2.x) * interp2.y, interp2.x * interp2.y, interp2.x * (1 - interp2.y), (1 - interp2.x) * (1 - interp2.y) );
		float sharpness = 1 + 5 * breakup;
		float4 edgeWeight;
		for( uint c = 0; c < 4; c++ )
		{
			float share = 0;
			for( uint o = 0; o < 4; o++ ) if ( matArray[o] == matArray[c] ) share += weight[o];
			float2 p = matCell * 1.3 + matArray[c] * 7.31;
			float n = EdgeNoise( p ) * 0.7 + EdgeNoise( p * 2.8 + 11.1 ) * 0.3;
			float pushed = saturate( share + n * 0.3 * breakup );
			edgeWeight[c] = weight[c] / max( share, 0.0001 ) * pow( pushed, sharpness );
		}
		float total = dot( edgeWeight, float4( 1, 1, 1, 1 ) );
		if ( total > 0.0001 ) edgeWeight /= total;
		else edgeWeight = weight;
		finalSurface = WeightSurfaces( surfaces, edgeWeight );
	}
	else
	{
		if ( matArray[0] != matArray[1] ) surfaces[0] = LerpSurface( surfaces[0], surfaces[1], interp2.x );
		if ( matArray[2] != matArray[3] ) surfaces[2] = LerpSurface( surfaces[3], surfaces[2], interp2.x );
		if ( matArray[0] != matArray[3] || matArray[1] != matArray[2] ) surfaces[0] = LerpSurface( surfaces[2], surfaces[0], interp2.y );

		finalSurface = surfaces[ 0 ];
	}

	float2 normalRG = finalSurface.normal.rg * 0.5 + 0.5;
	/*
	if ( detailLevel > 3 )
	{
		finalSurface.color = texGrass.SampleLevel( samplerPointClamp, float3(0.5,0.5,0), 9 ).rgb * 0.7;
		finalSurface.roughness = 1;
		finalSurface.metalness = 0;
	}
	else
	{
		float4 rotUVDX = uvdxy * 5;
		for( uint v = 0; v < 4; v++ )
		{
			for( uint u = 0; u < 4; u++ )
			{
				uint index = (v << 2) | u;
				float2x2 rotMat = { terrain_maskRotMat[ index ].x, terrain_maskRotMat[ index ].y, terrain_maskRotMat[ index ].z, terrain_maskRotMat[ index ].w };
				float2 rotUV = IN.uv * 5 + float2(u/4.0, v/4.0);
				rotUV = mul( rotMat, rotUV );
				float2 uvDX = mul( rotMat, rotUVDX.xy );
				float2 uvDY = mul( rotMat, rotUVDX.zw );
				float4 grassColor = texGrass.SampleGrad( samplerTriWrap, float3(rotUV,0), uvDX, uvDY );
				if ( grassColor.a > 0.5 ) finalSurface.color = grassColor.rgb*0.8;
			}
		}
	}
		*/

	output.color = float4( finalSurface.color, finalSurface.metalness );
	output.normal = float4( normalRG, finalSurface.roughness, finalSurface.ao );
	return output;
}