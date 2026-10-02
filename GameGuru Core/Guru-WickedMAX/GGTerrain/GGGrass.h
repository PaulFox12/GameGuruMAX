#ifndef _H_GGRASS
#define _H_GGRASS

#include <stdint.h>
#include "wiGraphicsDevice.h"
#include "wiScene.h"

#include "Shaders/GGGrassConstants.hlsli"

#define GGGRASS_PAINT_SPRAY         0
#define GGGRASS_PAINT_SPRAY_REMOVE  1
#define GGGRASS_PAINT_SPRAY_RESTORE 2

#define GGGRASS_INITIAL_LOD_DIST 1500

struct sUndoSysEventGrass;

namespace GGGrass
{
	struct GGGrassParams
	{
		int draw_enabled = 1;
		int shadow_range = 0; // 0=no shadows, 1-5=shadow cascade cutoff

		uint64_t paint_type = 0;  // 1 bit per grass type
		int paint_material = 0; // 0=auto, otherwise force the use of the grass for that material
		int paint_mode = GGGRASS_PAINT_SPRAY;
		int paint_density = 100; // 0 to 100

		float max_height = 30000;
		float min_height = -1000;

		float max_height_underwater = 1000;
		float min_height_underwater = -7000;

		float lod_dist = GGGRASS_INITIAL_LOD_DIST;
		int simplePBR = 0;
		float grass_scale = 40;
	};

	// these values must not be modified outside the tree module
	struct GGGrassInternalParams 
	{
		int prevMouseLeft = 0;
		int mouseLeftPressed = 0;
		int mouseLeftState = 0;
		int mouseLeftReleased = 0;
		float prevLodDist = GGGRASS_INITIAL_LOD_DIST;
	};

	extern GGGrassParams gggrass_save_params;
	extern GGGrassParams gggrass_global_params; // modify this anywhere

	void GGGrass_Init_Textures(LPSTR pRemoteGrassPath);
	void GGGrass_Init();
	void GGGrass_Update( wiScene::CameraComponent* camera, wiGraphics::CommandList cmd, bool bRenderTargetFocus );
	void GGGrass_Update_Painting( RAY ray );
	int GGGrass_UsingBrush();
	void GGGrass_BindGrassArray( uint32_t slot, wiGraphics::CommandList cmd );

	const char* GGGrass_GetTextureFilename( uint32_t matIndex, uint32_t grassIndex );
	const char* GGGrass_GetTextureShortName( uint32_t matIndex, uint32_t grassIndex );
	// the grass a tool plants by code (the spline rivers' banks): the selectable types (as the grass brush's), the type a
	// selectable one is on a terrain material (GGGRASS_NUM_TYPES or more: none there), and a type's texture loaded if it
	// isn't, as textures load only for the brush's types and those a loaded map holds
	uint32_t GGGrass_GetNumSelectableTypes();
	uint32_t GGGrass_GetRealIndex( uint32_t matIndex, uint32_t grassTypeIndex );
	void GGGrass_UseType( uint32_t realIndex );

	void GGGrass_SetPerformanceMode( uint32_t mode );

	void GGGrass_AddAll();
	void GGGrass_RemoveAll();
	void GGGrass_RestoreAll();

	uint32_t GGGrass_GetDataSize();
	int GGGrass_GetData( uint8_t* data ); // data must be allocated with a size of GGGrass_GetDataSize(), returns 1 on success
	int GGGrass_SetData( uint32_t size, uint8_t* data, sUndoSysEventGrass* pEvent = nullptr); // size must be equal to GGGrass_GetDataSize(), returns 1 on success

	void GGGrass_UpdateFlatArea( int mode, int type, float x, float z, float sx, float sz, float angle );
	void GGGrass_RestoreAllFlattened();
	int GGGrass_UpdateInstances();
	// GG: the grass map for a tool that writes it directly: 4096 x 4096 over the editable area, index z * 4096 + x; low 7 bits
	// 0 none, 1 auto, 2+ a type, bit 7 flattened. GGGrass_UpdateInstances after writing
	uint8_t* GGGrass_GetGrassMap();
	void GGGrass_DeferInstanceUpdates( int defer ); // 1 to start, 0 to end: flat area changes in between update the grass instances once, at the end
	void GGGrass_SetKillBox( int slot, float x, float y, float z, float halfX, float halfY, float halfZ, float yawDegrees ); // no blade is drawn inside, slot 0 to GGGRASS_MAX_KILLBOXES-1
	void GGGrass_ClearKillBox( int slot ); // -1 clears them all
	void GGGrass_SetKillCircles( const float* pCircles, int count ); // x, y, z, radius each; the nearest fill the kill shapes the boxes leave
	void GGGrass_SetShade( float root, float tip ); // a blade's light at its root and at its tip, 0.45 both by default
	void GGGrass_GetShade( float* pRoot, float* pTip );
	const char* GGGrass_GetTextureSource( int slice, int* pChanged ); // the file a grass slice was last uploaded from, "" none, 0 a bad slice; *pChanged as GGTerrain_CheckTextureSource
}

#endif // _H_GGGRAS