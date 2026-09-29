#ifndef _H_GGTREES
#define _H_GGTREES

#include <stdint.h>
#include "wiGraphicsDevice.h"
#include "wiScene.h"

#define GGTREES_PAINT_SPRAY         0
#define GGTREES_PAINT_ADD           1
#define GGTREES_PAINT_REMOVE        2
#define GGTREES_PAINT_MOVE          3
#define GGTREES_PAINT_SPRAY_REMOVE  4
#define GGTREES_PAINT_SCALE         5

struct sUndoSysEventTreeMove;

namespace GGTrees
{
	struct GGTreesParams
	{
		int draw_enabled = 0;
		int draw_shadows = 1;
		int tree_shadow_range = 3; // higher values draw tree shadows at greater distance, max=5

		int paint_mode = GGTREES_PAINT_SPRAY;
		uint64_t paint_tree_bitfield = 0x00100000; // 1 bit per tree, default pine
		int paint_density = 65; // 0 to 100
		float water_dist = 10;

		int paint_scale_random_low = 10; // 0 to 100
		int paint_scale_random_high = 245; // 0 to 100

		float lod_dist = 3000;
		float lod_dist_shadow = 2500;

		// width of the band where billboards and full detail trees cross fade (GGTREES_LOD_TRANSITION)
		float lod_transition = 500;
		float lod_transition_shadow = 500;

		// shadow cascades that draw full detail tree shadows (billboard shadows use tree_shadow_range)
		int tree_shadow_range_high = 3;

		int hide_until_update = 0;

	};

	// these values must not be modified outside the tree module
	struct GGTreesInternalParams 
	{
		int prevMouseLeft = 0;
		int mouseLeftPressed = 0;
		int mouseLeftState = 0;
		int mouseLeftReleased = 0;
		int mouseY = 0;
		float minTotalHeight = 0;
		float maxTotalHeight = 0;
		uint32_t tree_selected = 0xFFFFFFFF;
		int scaleMouseYStart = 0;
		int scaleStart = 0;
		uint32_t treeChunkUpdate = 0;
	};

	extern GGTreesParams ggtrees_global_params; // modify this anywhere

	extern int ggtrees_draw_enabled;

	struct GGTreePoint
	{
		float x;
		float y;
		float z;
		float scale;
	};

	int GGTrees_GetClosest( float x, float z, float radius, GGTreePoint** pOutPoints ); // returns the number of trees in pOutPoints, pOutPoints must be undefined it will be created
	int GGTrees_RayCast( RAY pickRay, float maxDist, float* outDist, uint32_t* treeID ); // returns 1 if hit, 0 if not. If hit then treeID will be populated
	void GGTrees_SetTreePosition( uint32_t treeID, float x, float z );
	
	uint32_t GGTrees_GetDataSize(); // number of floats required in data array
	void GGTrees_GetEmptyData( float* data ); // data as GGTrees_GetData, with every tree hidden and unplaced
	int GGTrees_GetData( float* data ); // data must be allocated with GGTrees_GetSculptDataSize() floats, returns 1 on success
	int GGTrees_SetData( float* data ); // number of floats must be equal to GGTrees_GetSculptDataSize(), returns 1 on success
	int GGTrees_GetSnapshot(uint8_t* data);

	void GGTrees_SetPerformanceMode( uint32_t mode );
	void GGTrees_ResetDistances(); // the stock tree distances, for a level saved without its own

	// values set from Lua override the performance presets until cleared; a distance or width of 0 or less,
	// or a negative cascade count, keeps the current value
	void GGTrees_SetLuaDistances( float lodDist, float lodDistShadow );
	void GGTrees_SetLuaTransitions( float lodTransition, float lodTransitionShadow );
	void GGTrees_SetLuaShadowCascades( int billboardCascades, int fullDetailCascades );
	void GGTrees_ApplyLuaOverrides();
	void GGTrees_ClearLuaOverrides();
	void GGTrees_Delete_Trees(float pickX, float pickZ, float radius);

	void GGTrees_Init();
	void GGTrees_UpdateFrustumCulling( wiScene::CameraComponent* camera );
	void GGTrees_Update( float camX, float camY, float camZ, wiGraphics::CommandList cmd, bool bRenderTargetFocus);
	void GGTrees_Update_Painting( RAY ray );
	int GGTrees_UsingBrush();
	uint32_t GGTrees_GetNumTypes();
	uint32_t GGTrees_GetNumHighDetail();
	void GGTrees_ChangeDensity(int density);
	void GGTrees_RepopulateInstances();
	int GGTrees_UpdateInstances(int accurate);
	void GGTrees_InvalidateHeights( float minX, float minZ, float maxX, float maxZ ); // call when the terrain height changes
	void GGTrees_HideAll();
	void GGTrees_DeselectHighlightedTree(void);
	void GGTrees_LockVisibility();

	const char* GGTrees_GetTextureName( uint32_t index );
	float GGTrees_GetImageScale( uint32_t index );

	void GGTrees_UpdateFlatArea( int mode, int type, float x, float z, float sx, float sz, float angle );
	void GGTrees_RestoreAllFlattened();

	// a blast in a game removes the trees in its area for the rest of the level: a tree whose trunk reaches into the box
	// (as GGGrass_SetKillBox's) or the circle (a radius on x and z, as far above and below) is no longer drawn, hit or
	// collided with, and is never saved as removed. GGTrees_RestoreKilled brings them all back
	void GGTrees_KillBox( float x, float y, float z, float halfX, float halfY, float halfZ, float yawDegrees );
	void GGTrees_KillCircle( float x, float y, float z, float radius );
	void GGTrees_RestoreKilled();

	bool GGTrees_GetDefaultDataV2(char *filename);
}

#endif // _H_GGTREES