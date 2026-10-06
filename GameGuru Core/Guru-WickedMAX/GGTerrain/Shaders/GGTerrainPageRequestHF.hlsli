#ifndef _H_GGTERRAIN_PAGE_REQUEST
#define _H_GGTERRAIN_PAGE_REQUEST

// GG: the virtual texture page a terrain pixel asks for, as the read-back takes it ((LOD + 1) << 16, page y << 8, page x):
// the prepass worked it out until the colour pass could (SetTerrainDepthOnlyPrepass), so both use this. uv: the page
// table's, uvPos: the world position's x and z, lodLevel: the chunk's level, pixelPos: the pixel, samp: the page sampler.
// Needs texPageTableFinal, the terrain constants and the page settings declared first
uint GGTerrainPageRequest( float2 uv, float2 uvPos, uint lodLevel, uint2 pixelPos, SamplerState samp )
{
	uint maxLayer = terrain_numLODLevels - 1;
	uint maxMip = GGTERRAIN_MAX_PAGE_TABLE_MIP; // not a full mip stack
	uint maxTotal = maxLayer + maxMip - 1;

	uint LOD = texPageTableFinal.CalculateLevelOfDetailUnclamped( samp, uv ); // LOD 0 is the highest level of detail
	uint origLOD = LOD;
	if ( origLOD <= 5 )
	{
		if ( pixelPos.x & 0x4 ) LOD += 1;
		if ( pixelPos.y & 0x4 ) LOD += 2;
	}
	else
	{
		if ( pixelPos.x & 0x4 ) LOD += 1;
	}
	LOD = clamp( LOD, lodLevel, maxTotal );

	uint2 pageUV;
	if ( LOD <= maxLayer )
	{
		float2 levelUV = uvPos - float2( terrain_LOD[ LOD ].x, terrain_LOD[ LOD ].z );
		levelUV *= terrain_LOD[ LOD ].size;
		levelUV.y = 1 - levelUV.y;

		pageUV = (uint2) (levelUV * 256);
		pageUV = clamp( pageUV, uint2(0,0), uint2(255,255) );
	}
	else
	{
		float2 levelUV = uvPos - float2( terrain_LOD[ maxLayer ].x, terrain_LOD[ maxLayer ].z );
		levelUV *= terrain_LOD[ maxLayer ].size;
		levelUV.y = 1 - levelUV.y;

		uint mipLevel = LOD - maxLayer;
		uint mipSize = 256 >> mipLevel;
		pageUV = (uint2) (levelUV * mipSize);
		pageUV = clamp( pageUV, uint2(0,0), uint2(mipSize-1,mipSize-1) );
	}

	// texture is cleared with 0, so increment mip level by 1
	uint request = (LOD + 1) << 16;
	request |= (pageUV.y << 8);
	request |= pageUV.x;
	return request;
}

#endif // _H_GGTERRAIN_PAGE_REQUEST
