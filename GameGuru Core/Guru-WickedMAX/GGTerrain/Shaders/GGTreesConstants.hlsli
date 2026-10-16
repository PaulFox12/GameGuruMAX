
#ifndef WI_SHADERINTEROP_RENDERER_H
#include "PBR/ShaderInterop_Renderer.h"
#endif

#define GGTREES_REFLECTANCE   0.004

// default widths of the billboard / full detail swap band, now set at run time through tree_lodTransition(Shadow)
#define GGTREES_LOD_TRANSITION    500.0
#define GGTREES_LOD_SHADOW_TRANSITION 500.0

#if defined(GGTREES_CONSTANTS_FULL_DECL) || !defined(__cplusplus)

#ifdef __cplusplus
	#define VAR_UNIT uint32_t
#else
	#define VAR_UNIT uint
#endif

struct TreeType
{
	float scaleX;
	float scaleY;
	// GG: the crown's extent in the model's own units, from the LOD0 branches mesh (SetTreeCrownShade)
	float crownMinY;
	float crownMaxY;
	float crownRadius;
	float padding1;
	float padding2;
	float padding3;
};

static const VAR_UNIT numTreeTypes = 38;

#ifdef __cplusplus
struct TreeCB
#else
cbuffer TreeCB : register( b2 )
#endif
{
	float4   tree_rotMatShadow;

	float4   tree_rotMat[ 8 ];	

	TreeType tree_type[ numTreeTypes ];
	
	float3   tree_playerPos;
	uint     tree_shadowFar; // GG: 1 for full detail tree shadows in cascades 2 and on

	float    tree_lodDist;
	float    tree_lodDistShadow;
	float    tree_lodTransition;
	float    tree_lodTransitionShadow;

	// GG: per-tree variety: the tint's strength 0 to 2 and whether billboards mirror (SetTreeVariety)
	float    tree_varietyTint;
	float    tree_varietyMirror;
	// GG: the leaves' and the bark's brightness, 0 to 2 (SetTreeShade)
	float    tree_shadeLeaves;
	float    tree_shadeBark;

	// GG: crown shading, 0 to 1, as occlusion (mode 0) or on the albedo (mode 1) (SetTreeCrownShade)
	float    tree_crownShade;
	float    tree_crownMode;
	float2   tree_crownPad;
};

uint GetTreeType( uint data ) { return (data >> 11) & 0x3F; }
uint GetTreeVariation( uint data ) { return (data >> 8) & 0x7; }
uint GetTreeHighlighted( uint data ) { return data & 0x4; }
float GetTreeScale( uint data ) { return ((data >> 16) & 0xFE) / 170.0 + 0.5; }

#ifndef __cplusplus
// sway speed set on its own (SetTreeWind's second value), otherwise tied to the amount as before
// GG: a full detail tree and its billboard cross fade over one band: both take the tree's own fade (0 all full detail, 1 all
// billboard, from the distance of its base) and the same threshold at each pixel, so each pixel shows exactly one of them
float TreeLODFade( float3 treePos, float3 viewPos, float lodDist, float transition )
{
	return saturate( (distance( treePos, viewPos ) - lodDist) / max( transition, 1.0 ) );
}

float TreeDither( float2 pixel )
{
	return frac( 52.9829189 * frac( dot( pixel, float2( 0.06711056, 0.00583715 ) ) ) );
}

// GG: per-tree variety. A hash of the tree's position (its instance offset, the same in every buffer that holds the tree)
// gives each tree a mirror bit and a tint, so a stand of one type stops reading as one stamped picture. Each vertex shader
// puts the hash in bits 24-31 of the data word it passes on, which nothing else uses (the scale ends at bit 23), so the
// billboard and the full detail tree carry the same tint across their cross fade
uint TreeVarietyHash( float3 offset )
{
	uint h = uint( int( floor( offset.x ) ) ) * 0x8da6b343u ^ uint( int( floor( offset.z ) ) ) * 0xd8163841u;
	h ^= h >> 13;
	h *= 0x5bd1e995u;
	h ^= h >> 15;
	return (h >> 8) & 0xFF;
}
uint TreeVarietyData( uint data, float3 offset ) { return (data & 0x00FFFFFF) | (TreeVarietyHash( offset ) << 24); }
bool TreeVarietyMirror( uint data ) { return tree_varietyMirror > 0 && (data & 0x01000000); }
// the tint: a brightness within 15 % either way and a lean of up to 6 % toward olive or as far toward a cool green, both
// scaled by the amount; the two leans are each other's inverse, so a stand's average colour stays where it was
float3 TreeVarietyTint( uint data, float amount )
{
	uint v = data >> 24;
	float bright = ((v >> 1) & 0xF) / 15.0 * 2.0 - 1.0;
	float lean = ((v >> 5) & 0x7) / 7.0 * 2.0 - 1.0;
	float3 tint = 1.0 + 0.15 * bright * amount;
	tint *= lerp( 1.0, float3( 1.06, 1.03, 0.90 ), saturate( lean ) * amount );
	return tint * lerp( 1.0, float3( 0.95, 0.98, 1.10 ), saturate( -lean ) * amount );
}

// GG: crown shading for a full detail tree's leaf or bark pixel from its place in the model (origPos): darkest deep inside
// the crown and low in it, the sunlit top rim left at 1; amount 0 gives 1 everywhere
float TreeCrownShade( uint treeType, float3 origPos, float amount )
{
	TreeType t = tree_type[ treeType ];
	float hc = saturate( (origPos.y - t.crownMinY) / max( t.crownMaxY - t.crownMinY, 1.0 ) );
	float rc = saturate( length( origPos.xz ) / max( t.crownRadius, 1.0 ) );
	float shade = 1.0 - amount * (0.45 * smoothstep( 0.2, 1.0, 1.0 - rc ) + 0.35 * smoothstep( 0.3, 1.0, 1.0 - hc ));
	return max( shade, 0.35 );
}

float TreeSwaySpeed()
{
    return (g_xFrame_TreeWindSpeed > 0) ? g_xFrame_TreeWindSpeed : g_xFrame_TreeWind * 6.0;
}

//PE: Animate the trees a bit.
float TreeWaveX(float posy, float posx)
{
    if (g_xFrame_TreeWind <= 0)
        return (0);
    const float swayspeed = TreeSwaySpeed(); // (0.85)
    const float swayamount = g_xFrame_TreeWind * 0.35; //0.075
    const float time = g_xFrame_Time;
    const float sdat = sin((time * (swayspeed * 1.5)) + posx) + cos((time * (swayspeed * 0.8)) + posx) + sin((time * (swayspeed * 1.2)));
    const float wave = sdat * 0.335;
    return (wave * (clamp((posy - 120) * 0.35, 0, posy) * swayamount));
}
float TreeWaveZ(float posy, float posx)
{
    if (g_xFrame_TreeWind <= 0)
        return (0);
    const float swayspeed = TreeSwaySpeed(); // (0.85)
    const float swayamount = g_xFrame_TreeWind * 0.20; //0.055
    const float time = g_xFrame_Time;
    const float sdat = sin((time * swayspeed) + posx) + cos((time * (swayspeed * 1.5)) + posx);
    const float wave = sdat * 0.5;
    return (wave * (clamp((posy - 120) * 0.35, 0, posy) * swayamount));
}
#endif
#endif // GGTREES_CONSTANTS_FULL_DECL