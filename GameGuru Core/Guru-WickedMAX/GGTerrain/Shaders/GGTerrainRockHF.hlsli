// steep rock (terrain_rockStrength above 0): a terrain texture laid on steep slopes in world space from three sides. The
// pages are made from above, so on a steep face each page texel covers its horizontal footprint and the texture is
// stretched down the face; this samples the texture where the face is instead, and blends it over the page's surface.
// Used by the terrain's final pass and its environment probe pass, which bind the terrain's texture arrays here

Texture2DArray<float4> texRockColor   : register( t55 );
Texture2DArray<float2> texRockNormal  : register( t56 );
#ifdef GGTERRAIN_USE_SURFACE_TEXTURE
Texture2DArray<float4> texRockSurface : register( t57 ); // R: occlusion, G: roughness, B: metalness
#endif

struct RockSurface
{
	float3 color;
	float3 normal;
	float occlusion;
	float roughness;
	float metalness;
};

// one side's sample, by its weight: its UV, and the world directions of its U and V (the normal map's X and Y)
void AddRockSide( inout RockSurface rock, float weight, float2 uv, float2 uvDX, float2 uvDY, float3 dirU, float3 dirV, float3 N, SamplerState samp )
{
	float3 arrayUV = float3( uv, terrain_rockMaterial );
	rock.color += texRockColor.SampleGrad( samp, arrayUV, uvDX, uvDY ).rgb * weight;
#ifdef GGTERRAIN_USE_SURFACE_TEXTURE
	float4 surface = texRockSurface.SampleGrad( samp, arrayUV, uvDX, uvDY );
	rock.occlusion += surface.r * weight;
	rock.roughness += surface.g * weight;
	rock.metalness += surface.b * weight;
#else
	rock.occlusion += weight;
	rock.roughness += 0.8 * weight;
#endif

	float3 tangentNormal;
	tangentNormal.xy = texRockNormal.SampleGrad( samp, arrayUV, uvDX, uvDY ).rg * 2 - 1;
	tangentNormal.z = sqrt( saturate( 1 - dot( tangentNormal.xy, tangentNormal.xy ) ) );

	// U and V laid along the surface, so the normal map tilts the surface's own normal
	dirU = normalize( dirU - N * dot( N, dirU ) );
	dirV = normalize( dirV - N * dot( N, dirV ) );
	rock.normal += normalize( dirU * tangentNormal.x + dirV * tangentNormal.y + N * tangentNormal.z ) * weight;
}

// the rock over the page's surface where the terrain is steep: geometricNormal is the mesh's normal, posDX and posDY the
// world position's derivatives (taken outside any branch); normal, colorMetalness and normalRoughnessAO as the page gave
void GGTerrainApplyRock( float3 worldPos, float3 geometricNormal, float3 posDX, float3 posDY, SamplerState samp, inout float3 normal, inout float4 colorMetalness, inout float4 normalRoughnessAO )
{
	float3 N = normalize( geometricNormal );
	float slope = saturate( (1 - abs( N.y ) - terrain_rockStart) * terrain_rockTransition );
	slope = slope * slope * (3 - 2 * slope);

	[branch]
	if ( terrain_rockStrength > 0 && slope > 0 )
	{
		// three sides by the normal, sharpened so a face takes mostly one; the pages' own directions from above (U along
		// +X, V along -Z), down the face on the sides so a texture stands upright
		float3 weights = pow( abs( N ), 4 );
		weights /= weights.x + weights.y + weights.z;

		RockSurface rock = (RockSurface) 0;
		float scale = terrain_rockScale;
		[branch] if ( weights.x > 0.01 ) AddRockSide( rock, weights.x, float2( worldPos.z, -worldPos.y ) * scale, float2( posDX.z, -posDX.y ) * scale, float2( posDY.z, -posDY.y ) * scale, float3( 0, 0, 1 ), float3( 0, -1, 0 ), N, samp );
		[branch] if ( weights.y > 0.01 ) AddRockSide( rock, weights.y, float2( worldPos.x, -worldPos.z ) * scale, float2( posDX.x, -posDX.z ) * scale, float2( posDY.x, -posDY.z ) * scale, float3( 1, 0, 0 ), float3( 0, 0, -1 ), N, samp );
		[branch] if ( weights.z > 0.01 ) AddRockSide( rock, weights.z, float2( worldPos.x, -worldPos.y ) * scale, float2( posDX.x, -posDX.y ) * scale, float2( posDY.x, -posDY.y ) * scale, float3( 1, 0, 0 ), float3( 0, -1, 0 ), N, samp );
		float sampled = (weights.x > 0.01 ? weights.x : 0) + (weights.y > 0.01 ? weights.y : 0) + (weights.z > 0.01 ? weights.z : 0);
		rock.color /= sampled;
		rock.occlusion /= sampled;
		rock.roughness /= sampled;
		rock.metalness /= sampled;
		float3 rockNormal = normalize( rock.normal );
		rockNormal = normalize( lerp( N, rockNormal, terrain_bumpiness ) );

		// where it starts, the rock's raised parts (light in its occlusion) show first and its cracks fill last
		float rockWeight = slope + rock.occlusion * 0.5;
		float pageWeight = (1 - slope) + 0.25;
		float top = max( rockWeight, pageWeight ) - 0.2;
		rockWeight = max( rockWeight - top, 0 );
		pageWeight = max( pageWeight - top, 0 );
		float amount = rockWeight / (rockWeight + pageWeight) * terrain_rockStrength;

		colorMetalness.rgb = lerp( colorMetalness.rgb, rock.color, amount );
		colorMetalness.a = lerp( colorMetalness.a, rock.metalness, amount );
		normalRoughnessAO.b = lerp( normalRoughnessAO.b, rock.roughness, amount );
		normalRoughnessAO.a = lerp( normalRoughnessAO.a, rock.occlusion, amount );
		normal = normalize( lerp( normal, rockNormal, amount ) );
	}
}
