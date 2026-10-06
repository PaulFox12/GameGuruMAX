#ifndef _H_GRASS_CONSTANTS
#define _H_GRASS_CONSTANTS

#define GGGRASS_REFLECTANCE       0.02
#define GGGRASS_SCALE             40.0
#define GGGRASS_LOD_TRANSITION    2500.0

//#if defined(GGGRASS_CONSTANTS_FULL_DECL) || !defined(__cplusplus)

#ifdef __cplusplus
	#define VAR_UNIT uint32_t
	#define VAR_MAT2X2 float4
#else
	#define VAR_UNIT uint
	#define VAR_MAT2X2 float2x2
#endif

struct GrassType
{
	float scaleFactor;
	float cosTime; // not a grass type parameter, but making use of unused space
	float padding1;
	float padding2;
};

#define GGGRASS_NUM_TYPES 46    // must be larger than 31 to have enough cosTime entries
#define GGGRASS_NUM_SELECTABLE_TYPES 22

#define GGGRASS_FLAGS_SIMPLE_PBR  0x0001 // increase performance by simplifying the PBR shader but with lower quality

#define GGGRASS_MAX_KILLBOXES 8 // boxes set from Lua (SetGrassKillBox) that no blade is drawn in, such as under a vehicle
#define GGGRASS_MAX_KILLSHAPES 32 // those boxes, then the circles the engine clears (under a projected decal) whose edge is nearest

#ifdef __cplusplus
struct GrassCB
#else
cbuffer GrassCB : register( b2 )
#endif
{
	float4 grass_rotMat[ 32 ];

	GrassType grass_type[ GGGRASS_NUM_TYPES ];

	float grass_lodDist;
	uint grass_flags;
	float grass_scale;
	float grass_windTime; // seconds, for the wind ripple

	float4 grass_wind; // xy: wind direction on x and z (normalised), z: sway amount (the tree wind), w: sway speed

	uint grass_killbox_count;
	float grass_shadeRoot; // a blade's light at its root and at its tip (SetGrassShade), 0.45 both as the engine has drawn it
	float grass_shadeTip;
	float grass_fadeBand; // the dithered band past grass_lodDist where the cards thin out (SetGrassFadeBand), units
	float4 grass_killbox_centre[ GGGRASS_MAX_KILLSHAPES ]; // xyz: world centre, w: cos of the box's yaw
	float4 grass_killbox_half[ GGGRASS_MAX_KILLSHAPES ]; // xyz: half size along the box's own axes, w: sin of its yaw; x below 0 is a circle of radius -x
};

// shader only
#ifndef __cplusplus
	uint GetGrassType( uint data ) { return data & 0xFF; }
	uint GetGrassVariation( uint data ) { return (data >> 8) & 31; } // must result in a value less than GGGRASS_NUM_TYPES to have enough cosTime entries

	// world-space bend of a blade vertex from the wind: the blade leans along the wind direction, most at the tip, and
	// the lean swells and eases as gusts roll across the field; the vertex also drops a little so the blade keeps
	// roughly its length. Zero when there is no wind, so calm grass is unchanged.
	// heightFraction is 0 at the root and 1 at the tip, bladeHeight the vertex's height above the root in world units
	float3 GrassWindOffset( float2 rootPosXZ, float heightFraction, float bladeHeight )
	{
		if ( grass_wind.z <= 0 || bladeHeight <= 0 ) return float3( 0, 0, 0 );
		float phase = dot( rootPosXZ, grass_wind.xy ) * (6.2832 / 1200.0); // one gust every ~1200 units along the wind
		float gust = 0.6 + 0.4 * sin( grass_windTime * grass_wind.w * 1.3 - phase );
		float lean = grass_wind.z * gust * heightFraction * bladeHeight;
		return float3( grass_wind.x * lean, -0.5 * lean * lean / bladeHeight, grass_wind.y * lean );
	}

	// true when a blade's root lies inside one of the kill boxes (a box turned by its yaw about Y) or kill circles (a
	// radius on x and z, half height on y), so the blade is not drawn: the caller puts every vertex at the root
	bool GrassInKillBox( float3 rootPos )
	{
		for ( uint i = 0; i < grass_killbox_count; i++ )
		{
			float3 d = rootPos - grass_killbox_centre[ i ].xyz;
			if ( grass_killbox_half[ i ].x < 0 )
			{
				float radius = -grass_killbox_half[ i ].x;
				if ( dot( d.xz, d.xz ) <= radius * radius && abs( d.y ) <= grass_killbox_half[ i ].y ) return true;
				continue;
			}
			float c = grass_killbox_centre[ i ].w;
			float s = grass_killbox_half[ i ].w;
			float3 local = float3( d.x * c - d.z * s, d.y, d.x * s + d.z * c );
			if ( all( abs( local ) <= grass_killbox_half[ i ].xyz ) ) return true;
		}
		return false;
	}
#endif

#ifndef __cplusplus
	// a card vertex's world position, worked out the same way in the prepass and the colour pass to the last bit (precise),
	// as the colour pass may draw only where the prepass's depth is equal (SetGrassEqualDepth); posOrig gets the card's size
	// before it is turned. position: the card corner (x -0.5 to 0.5, y 0 root to 1 tip), offset: the card's root
	float4 GrassCardWorldPos( float2 position, float3 offset, uint data, uint instanceID, out float4 posOrig )
	{
		uint grassType = GetGrassType( data );
		uint index = GetGrassVariation( data );

		precise float4 size = float4( position * grass_scale, 0, 0 );
		size.x *= grass_type[ grassType ].scaleFactor;
		size.y *= (instanceID & 0x0F) * 0.02 + 0.8;
		posOrig = size;

		float2x2 rotMat = { grass_rotMat[ index ].x, grass_rotMat[ index ].y, grass_rotMat[ index ].z, grass_rotMat[ index ].w };

		precise float4 pos;
		pos.xz = size.xz;
		pos.x += grass_type[ index ].cosTime * position.y * 2;
		pos.xz = mul( rotMat, pos.xz );
		pos.y = size.y;
		pos.w = 1;

		pos.xyz += offset;
		pos.xyz += GrassWindOffset( offset.xz, position.y, size.y );
		if ( GrassInKillBox( offset ) ) pos.xyz = offset; // inside a kill box: the whole blade at its root, so nothing is drawn
		return pos;
	}
#endif

//#endif // GGTREES_CONSTANTS_FULL_DECL

#endif // _H_GRASS_CONSTANTS