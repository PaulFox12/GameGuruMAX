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
#endif

//#endif // GGTREES_CONSTANTS_FULL_DECL

#endif // _H_GRASS_CONSTANTS