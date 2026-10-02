//----------------------------------------------------
//--- GAMEGURU - M-Splines
//----------------------------------------------------

// GG: splines drawn on the terrain in the editor, edited in the 3D view while Edit Splines is on in Terrain Tools (Roads
// and Rivers). A spline is a list of nodes on the ground joined straight, smoothly (each node's tangent along its
// neighbours, each handle a third of its segment) or by Bezier handles. Nodes of different splines can share a junction
// (a T junction or a crossing): they keep one position, so dragging one drags them all. Saved with the level in map.spl
//
// In the 3D view:
// - click the terrain to add a node at the end (or the start, with the first node selected), and drag it while held;
//   while drawing (New Spline, or a node just added), a click on another spline's node or curve adds the next node there
//   joined to it, and on the spline's own other end closes it; Esc stops drawing
// - drag a node, or a Bezier handle (Alt-drag breaks a handle pair apart)
// - a dragged node snaps to a node of another spline, or onto its curve (a node is inserted there), and joins it on
//   release; an end dropped on the spline's other end closes it; Alt-drag a joined node pulls it out of the junction
// - Shift+click the curve to insert a node, Ctrl+click a node to delete it
// - click the selected spline's curve to pick the segment between two nodes; Subdivide Segment splits it, Subdivide All
//   splits every segment, each into the chosen number of pieces
//
// A Road spline is baked into the terrain's own maps, so the level shows it anywhere: its sculpt heights (a smoothed,
// grade limited profile, flat across the carriageway, blended to the ground over the shoulders), its paint (a texture slot
// for the carriageway and optionally the shoulders), its grass (cleared) and its trees (hidden). What each texel held and
// what the bake wrote are kept, so a bake is undone by putting back only what still holds the bake's value: work done over
// a road afterwards stays. A changed road is baked again once the mouse is let go, with every spline joined to it: the
// joined ones are restored last first and baked in list order, a junction pins the later road to the earlier one's
// surface there, and a later road's shoulders leave an earlier road's carriageway alone. A River spline is baked the same
// way as a channel: a flat bed below the averaged ground (never rising downstream when Downhill Only), banks up to the
// ground, a tributary pinned to an earlier river's bed where they join. A baked river gets a water surface of its own
// (WickedCall_CreateWaterSurface, the Water Object shader) at its water depth above the bed (no higher than its lower bank,
// reaching to the banks and ending at the sea), flowing from the first node to the last, its look the
// main water's or its own; spline_waterheightat gives the water height anywhere (Lua GetWaterHeightAt), and the whole map
// navmesh keeps soldiers out of the river water as out of the sea. Where the river's slope changes sharply it is turbulent:
// white water and choppy, and it runs faster down a steep run (spline_riverturbulenceat, Lua GetRiverTurbulenceAt)

#include "stdafx.h"
#include "gameguru.h"
#include "M-Splines.h"

#include "..\Imgui\imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "..\Imgui\imgui_internal.h"
#include "..\Imgui\imgui_impl_win32.h"
#include "..\Imgui\imgui_gg_dx11.h"

#include "GGTerrain\GGTerrain.h"
#include "GGTerrain\GGTrees.h"
#include "GGTerrain\GGGrass.h"

#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <float.h>

extern bool bImGuiGotFocus;
extern bool bImGuiRenderTargetFocus;
extern bool bForceKey2;
extern bool bExternal_Entities_Window; // the object library
extern int iDisplayLibraryType, iDisplayLibrarySubType, iLibraryStingReturnToID, iSelectedLibraryStingReturnID;
extern cstr sSelectedLibrarySting;
extern ImVec2 renderTargetAreaPos;
extern ImVec2 renderTargetAreaSize;
bool Convert3DLineTo2D( float x1, float y1, float z1, float x2, float y2, float z2, ImVec2* pA, ImVec2* pB );
float BT_GetGroundHeight( unsigned long value, float x, float z );
extern int iCurrentTextureForPaint;
extern int sTerrainTexturesID[32]; // the Paint palette: each entry's image, and the texture slot it paints
extern int sTerrainSelectionID[32];

#define SPLINE_CURVE_LINEAR 0
#define SPLINE_CURVE_SMOOTH 1
#define SPLINE_CURVE_BEZIER 2

#define SPLINE_KIND_NONE 0
#define SPLINE_KIND_ROAD 1
#define SPLINE_KIND_RIVER 2

#define SPLINE_NODE_BROKEN 1 // the node's Bezier handles move apart

#define SPLINE_FILE_MAGIC 0x50534747 // 'GGSP'
#define SPLINE_FILE_VERSION 18 // 2: road settings and the bake; 3: river settings; 4: the river's water; 5: Raise Low Banks; 6: See Depth; 7: rapids; 8: Bank Foam; 9: placement layers; 10: layer names, keep apart, freeze, Calm River End; 11: Follow Slope; 12: a layer's name is its entity's unless set; 13: Jitter Across; 14: the preset; 15: each segment's curve; 16: Lay Flat; 17: Wade Depth; 18: road markings

#define SPLINE_MAP_SIZE 4096 // the terrain's sculpt, paint and grass maps over the editable area
#define SPLINE_UNITS_PER_M 39.37f

#define SPLINE_PICK_NODE 10.0f // pixels
#define SPLINE_PICK_CURVE 8.0f
#define SPLINE_SNAP_NODE 14.0f
#define SPLINE_SNAP_CURVE 10.0f
#define SPLINE_DRAW_LIFT 15.0f // drawn this far above the ground

struct sSplineNode
{
	float x = 0, y = 0, z = 0;
	float inX = 0, inZ = 0; // Bezier handles, offsets from the node on the ground plane
	float outX = 0, outZ = 0;
	int flags = 0;
	int junction = 0; // nodes of other splines with the same junction share this node's position
	int segCurve = -1; // the curve of the segment from this node to the next: SPLINE_CURVE_*, or -1 the spline's
	int segMark = -1; // the markings of the segment from this node to the next: SPLINE_SEGMARK_*, or -1 the road's
};

// a river's water over a terrain texel: its height, and how turbulent it is there (0-1)
struct sRiverTexel
{
	float height = 0;
	float turbulence = 0;
	float flowX = 0, flowZ = 0; // the current, units a second downstream
	float wade = 0; // the ground under it walkable this far down (the river's Wade Depth)
};

// a placement layer: copies of an entity along a road or a river, spacing apart, on its sides or its centre
#define SPLINE_SIDE_BOTH 0
#define SPLINE_SIDE_LEFT 1
#define SPLINE_SIDE_RIGHT 2
#define SPLINE_SIDE_ALTERNATE 3
#define SPLINE_SIDE_CENTRE 4
#define SPLINE_FACE_MIRRORED 0 // each side turned to face the other across it (as the Long Bien bridge's lamps)
#define SPLINE_FACE_ALONG 1
#define SPLINE_FACE_RANDOM 2

// a road's centre line (Markings)
#define SPLINE_MARK_NONE 0
#define SPLINE_MARK_DASHED 1
#define SPLINE_MARK_SOLID 2
#define SPLINE_MARK_DOUBLE 3
#define SPLINE_MARK_SOLIDDASHED 4 // solid on the left (seen from the first node), dashed on the right
// a segment's own markings over the road's
#define SPLINE_SEGMARK_ROAD -1
#define SPLINE_SEGMARK_NONE 0 // no lines
#define SPLINE_SEGMARK_SOLID 1 // the centre line solid (a bridge, a crest)
#define SPLINE_SEGMARK_DASHED 2 // the centre line dashed
#define SPLINE_ENTITY_TAG 0x4E4C5053 // 'SPLN' in eleprof.iObjectReserved1; iObjectReserved2 the spline's id, 3 the layer
struct sSplineLayer
{
	char entity[260] = ""; // the .fpe, relative to the entity bank
	float spacing = 1213.0f;
	float start = 0.0f; // the first this far along
	float offset = 20.0f; // out from the edge (a road's carriageway, a river's bed), or across from the centre
	float turn = 0.0f; // degrees more
	float height = 0.0f;
	float scaleMin = 100.0f, scaleMax = 100.0f; // percent
	float jitter = 0.0f; // each moved up to this far along the spline, at random
	float minTurbulence = 0.0f; // a river: only where its water is at least this turbulent (rapids), 0 anywhere
	int side = SPLINE_SIDE_BOTH;
	int facing = SPLINE_FACE_MIRRORED;
	int enabled = 1;
	char name[64] = ""; // each placed one named this (else the entity's own name) with __01, __02 ... after it, on from the highest in the level
	float keepApart = 600.0f; // none placed this close to one another spline (earlier in the list) placed of the same entity
	int frozen = 0; // its entities stay as they are, no longer placed again
	int followSlope = 0; // each tilted to the ground under it (decals, flat things), else upright
	float jitterAcross = 0.0f; // each moved up to this far across, at random (cars spread over both lanes)
	int layFlat = 0; // the model turned onto its back first, a wall decal laid on the ground: 1 its face (-Z) up, 2 its back up
};

// an entity a layer placed, where it was put (found again by its tag and place)
struct sSplinePlaced
{
	int layer = 0;
	float x = 0, y = 0, z = 0;
};

// a road's settings, in units (the panel shows metres)
struct sSplineRoad
{
	float width = 670.0f; // the carriageway (the Long Bien roadway, 17 m)
	float shoulder = 240.0f; // each side, blended from the road to the ground
	float smoothing = 1575.0f; // the profile is averaged over this length
	float maxGrade = 12.0f; // percent
	float crown = 0.0f; // the centre this far above the carriageway's edges
	float grassMargin = 40.0f; // grass cleared this far past the carriageway's edge
	float treeMargin = 80.0f; // trees hidden this far past it
	int material = 0; // terrain texture slot + 1 for the carriageway, 0 none
	int edgeMaterial = 0; // the same for the shoulders
	int autoApply = 1; // baked again when it changes
	// its painted lines (spline_markings), drawn into the terrain's pages rather than baked
	int markCentre = 0; // SPLINE_MARK_*
	int markCentreYellow = 0; // the centre line yellow, else white
	int markLanes = 1; // lanes each way, dashed lines between them
	int markEdges = 0; // solid edge lines
	int markEdgeYellow = 0;
	float markEdgeInset = 8.0f; // the edge lines this far in from the carriageway's edge (0.2 m)
	float markLineWidth = 3.94f; // the centre and lane lines (10 cm)
	float markEdgeWidth = 7.87f; // the edge lines (20 cm)
	float markDash = 118.1f; // a dash (3 m)
	float markGap = 354.3f; // and the gap after it (9 m)
	float markBendRadius = 0.0f; // the centre line solid where the road bends tighter than this, 0 never
	float markWear = 0.0f; // 0 fresh paint, 1 worn away in patches
};

// a river's settings, in units (the panel shows metres)
struct sSplineRiver
{
	float bedWidth = 600.0f; // the flat bed
	float depth = 240.0f; // the bed this far below the smoothed ground
	float banks = 400.0f; // each side, blended from the bed up to the ground
	float smoothing = 1575.0f; // the bed is averaged over this length
	float grassMargin = 120.0f; // grass cleared this far past the bed's edge
	float treeMargin = 160.0f; // trees hidden this far past it
	int bedMaterial = 0; // terrain texture slot + 1 for the bed, 0 none
	int bankMaterial = 0; // the same for the banks
	int downhill = 1; // the bed never rises from the first node to the last
	int autoApply = 1; // baked again when it changes
	float waterDepth = 180.0f; // the water surface this far above the bed
	int mainLook = 1; // the water looks like the main water (its colour, flow and waves), else as set here
	float colour[3] = { 0.04f, 0.08f, 0.17f };
	float clarity = 0.75f; // how much of the bed shows through in the shallows (1 - the water fog's minimum)
	float seeDepth = 11500.0f; // how deep the water is before its colour hides the bed (the water fog's max)
	float flow = 1.0f; // flow speed
	float waves = 1.0f; // wave distortion
	float foam = 0.3f; // foam where the water meets the banks, 0 none
	float ripples = 1.0f; // ripple size
	int raiseBanks = 1; // where the ground beside it is lower than the water, a bank is raised to hold it
	float rapids = 1.0f; // white water where the river's slope changes sharply (and a little on steep runs), 0 none
	float steepFlow = 1.5f; // how much faster the water runs on a steep slope, 0 the same everywhere
	int calmEnd = 1; // the rapids fade out before its last node (they always fade before the sea)
	float wadeDepth = 35.4f; // the navmesh walks its bed where the water is no deeper than this (0.9 m), 0 none of it
};

// a terrain texel a bake wrote: what it held, and what the bake left there
#define SPLINE_TEXEL_CARRIAGEWAY 1
#define SPLINE_TEXEL_HEIGHT 2
#define SPLINE_TEXEL_MATERIAL 4
#define SPLINE_TEXEL_GRASS 8
struct sSplineBakeTexel
{
	uint16_t x = 0, z = 0;
	uint8_t flags = 0;
	uint8_t typeBefore = 0, typeAfter = 0;
	uint8_t matBefore = 0, matAfter = 0;
	uint8_t grassBefore = 0, grassAfter = 0;
	float heightBefore = 0, heightAfter = 0;
};

// a tree a bake hid: its slot and the tree that was in it
struct sSplineBakeTree
{
	uint32_t id = 0;
	float x = 0, z = 0;
	uint32_t data = 0;
};

struct sSpline
{
	int id = 0;
	char name[64] = "";
	int kind = SPLINE_KIND_NONE;
	int curve = SPLINE_CURVE_SMOOTH;
	int closed = 0;
	std::vector<sSplineNode> nodes;
	sSplineRoad road;
	sSplineRiver river;
	float wetFraction = -1.0f; // a river: how much of its bed is below the level's water line, -1 not known yet
	uint64_t waterEntity = 0; // a river: its water surface (WickedCall_CreateWaterSurface), 0 none
	uint64_t waterShape = 0, waterLook = 0; // what the surface was built from, and the look it was given
	std::vector<std::pair<uint32_t, sRiverTexel>> waterTexels; // the terrain texels under its water, its height and turbulence there
	uint64_t bakedSignature = 0; // the spline as last baked (spline_signature)
	int bakeCount = 0; // bakes since the level loaded, so a bake again rebuilds the river's water
	float waterLowered = 0.0f; // a river: how much of it has its water lowered by a bank lower than the water
	std::vector<sSplineBakeTexel> baked;
	std::vector<sSplineBakeTree> bakedTrees;
	std::vector<sSplineLayer> layers;
	std::vector<sSplinePlaced> placed;
	uint64_t placedSignature = 0; // the layers and the bake they were placed for
	char preset[64] = ""; // the preset it was set from (Roads and Rivers' Preset), and its settings then: edited if they differ
	uint64_t presetSignature = 0;
};

struct sSplinePoint
{
	float x, y, z;
	int seg;
	float t;
};

std::vector<sSpline> g_Splines;
int g_iSplineSelected = -1;
int g_iSplineNodeSelected = -1;
int g_iSplineSegSelected = -1; // the segment from this node to the next, picked by clicking the selected spline's curve
int g_iSplinePieces = 2; // Subdivide splits a segment into this many
int g_iSplineNextID = 1;
int g_iSplineNextJunction = 1;
bool g_bSplineEditMode = false;
int g_iSplinePanelFrame = -10;

// the drag in progress: a node, or a node's handle (1 in, 2 out)
static int iDragSpline = -1, iDragNode = -1, iDragHandle = 0;
// where the dragged node would snap: a node of another spline, or a point on its curve (iSnapNode -1)
static int iSnapSpline = -1, iSnapNode = -1, iSnapSeg = -1;
static float fSnapT = 0, fSnapX = 0, fSnapY = 0, fSnapZ = 0;
// Connect to another spline's end: the next click on an end node joins the two with a new segment
static bool bConnectMode = false;
// Apply: this spline is baked now, whether or not it changed
static int iApplySpline = -1;
// the frame a level's splines were loaded: nothing is baked or placed again for a while after it, so a bake never runs on
// a level still settling (a bake at load captured the wrong ground under a road, and a re-place put its entities out of sight)
static int g_iSplineLoadFrame = -1000;
#define SPLINE_LOAD_SETTLE_FRAMES 60
// an undo or redo put these splines (ids) back as they were baked: they are baked now, whether or not they Update as I Edit
static std::unordered_set<int> g_SplineForceBake;
// the object library is open to pick a layer's entity
static bool g_bLibraryPickEntity = false;
// drawing: nodes are being added to the selected spline, so a click on another spline's node or curve joins it
static bool bDrawing = false;
// the rivers' water over the terrain texels (the highest where rivers meet), for the water height and the navmesh
static std::unordered_map<uint32_t, sRiverTexel> g_RiverWater;
// after a level load the water waits this many frames (and for the terrain), so the terrain has taken the level's settings
static int g_iSplineWaterWait = 0;

//
// The curve
//

static int spline_segments( const sSpline& s )
{
	int n = (int)s.nodes.size();
	if ( n < 2 ) return 0;
	return (s.closed && n > 2) ? n : n - 1;
}

static int spline_wrap( const sSpline& s, int i )
{
	int n = (int)s.nodes.size();
	return ((i % n) + n) % n;
}

static bool spline_isend( const sSpline& s, int i )
{
	if ( s.closed && s.nodes.size() > 2 ) return false;
	return i == 0 || i == (int)s.nodes.size() - 1;
}

// a node's handles as the curve uses them: none on a straight spline, along the neighbours on a smooth one
// the curve of the segment from node seg to the next: its own if set, else the spline's
static int spline_segcurve( const sSpline& s, int seg )
{
	if ( seg < 0 || seg >= (int)s.nodes.size() ) return s.curve;
	const int curve = s.nodes[ seg ].segCurve;
	return (curve >= SPLINE_CURVE_LINEAR && curve <= SPLINE_CURVE_BEZIER) ? curve : s.curve;
}

// a node's handles: its in handle by the curve of the segment ending at it, its out handle by the one starting there
static void spline_handles( const sSpline& s, int i, float* pInX, float* pInZ, float* pOutX, float* pOutZ )
{
	*pInX = *pInZ = *pOutX = *pOutZ = 0;
	const int n = (int)s.nodes.size();
	const sSplineNode& p = s.nodes[ i ];
	const int inCurve = (i > 0 || (s.closed && n > 2)) ? spline_segcurve( s, spline_wrap( s, i - 1 ) ) : s.curve;
	const int outCurve = spline_segcurve( s, i );
	if ( inCurve == SPLINE_CURVE_BEZIER ) { *pInX = p.inX; *pInZ = p.inZ; }
	if ( outCurve == SPLINE_CURVE_BEZIER ) { *pOutX = p.outX; *pOutZ = p.outZ; }
	if ( (inCurve != SPLINE_CURVE_SMOOTH && outCurve != SPLINE_CURVE_SMOOTH) || n < 2 ) return;
	const bool bWrap = s.closed && n > 2;
	const sSplineNode& prev = (i > 0 || bWrap) ? s.nodes[ spline_wrap( s, i - 1 ) ] : p;
	const sSplineNode& next = (i < n - 1 || bWrap) ? s.nodes[ spline_wrap( s, i + 1 ) ] : p;
	float dx = next.x - prev.x, dz = next.z - prev.z;
	float len = sqrtf( dx * dx + dz * dz );
	if ( len < 0.001f ) return;
	dx /= len;
	dz /= len;
	const float before = sqrtf( (p.x - prev.x) * (p.x - prev.x) + (p.z - prev.z) * (p.z - prev.z) ) / 3.0f;
	const float after = sqrtf( (next.x - p.x) * (next.x - p.x) + (next.z - p.z) * (next.z - p.z) ) / 3.0f;
	if ( inCurve == SPLINE_CURVE_SMOOTH ) { *pInX = -dx * before; *pInZ = -dz * before; }
	if ( outCurve == SPLINE_CURVE_SMOOTH ) { *pOutX = dx * after; *pOutZ = dz * after; }
}

// a segment's four control points on the ground plane (x, z pairs)
static void spline_controls( const sSpline& s, int seg, float* c )
{
	const int ia = seg, ib = spline_wrap( s, seg + 1 );
	const sSplineNode& a = s.nodes[ ia ];
	const sSplineNode& b = s.nodes[ ib ];
	float aInX, aInZ, aOutX, aOutZ, bInX, bInZ, bOutX, bOutZ;
	spline_handles( s, ia, &aInX, &aInZ, &aOutX, &aOutZ );
	spline_handles( s, ib, &bInX, &bInZ, &bOutX, &bOutZ );
	c[0] = a.x; c[1] = a.z;
	c[2] = a.x + aOutX; c[3] = a.z + aOutZ;
	c[4] = b.x + bInX; c[5] = b.z + bInZ;
	c[6] = b.x; c[7] = b.z;
	if ( spline_segcurve( s, seg ) == SPLINE_CURVE_LINEAR )
	{
		c[2] = a.x + (b.x - a.x) / 3.0f; c[3] = a.z + (b.z - a.z) / 3.0f;
		c[4] = a.x + (b.x - a.x) * 2.0f / 3.0f; c[5] = a.z + (b.z - a.z) * 2.0f / 3.0f;
	}
}

static void spline_point( const sSpline& s, int seg, float t, float* pX, float* pZ )
{
	float c[8];
	spline_controls( s, seg, c );
	const float u = 1.0f - t;
	const float b0 = u * u * u, b1 = 3 * u * u * t, b2 = 3 * u * t * t, b3 = t * t * t;
	*pX = b0 * c[0] + b1 * c[2] + b2 * c[4] + b3 * c[6];
	*pZ = b0 * c[1] + b1 * c[3] + b2 * c[5] + b3 * c[7];
}

static float spline_groundy( float x, float z )
{
	return BT_GetGroundHeight( 0, x, z );
}

// points along the curve about spacing apart, each with its segment and place on it
static void spline_sample( const sSpline& s, float spacing, std::vector<sSplinePoint>& out )
{
	out.clear();
	const int segs = spline_segments( s );
	if ( segs == 0 )
	{
		if ( s.nodes.size() == 1 ) out.push_back( { s.nodes[0].x, spline_groundy( s.nodes[0].x, s.nodes[0].z ), s.nodes[0].z, 0, 0.0f } );
		return;
	}
	for ( int seg = 0; seg < segs; seg++ )
	{
		float c[8];
		spline_controls( s, seg, c );
		float polygon = 0;
		for ( int k = 0; k < 3; k++ ) polygon += sqrtf( (c[k*2+2] - c[k*2]) * (c[k*2+2] - c[k*2]) + (c[k*2+3] - c[k*2+1]) * (c[k*2+3] - c[k*2+1]) );
		int steps = (int)(polygon / spacing) + 1;
		if ( steps > 512 ) steps = 512;
		for ( int k = 0; k < steps; k++ )
		{
			sSplinePoint p;
			p.seg = seg;
			p.t = (float)k / steps;
			spline_point( s, seg, p.t, &p.x, &p.z );
			p.y = spline_groundy( p.x, p.z );
			out.push_back( p );
		}
	}
	sSplinePoint p;
	p.seg = segs - 1;
	p.t = 1.0f;
	spline_point( s, segs - 1, 1.0f, &p.x, &p.z );
	p.y = spline_groundy( p.x, p.z );
	out.push_back( p );
}

static float spline_length( const sSpline& s )
{
	std::vector<sSplinePoint> points;
	spline_sample( s, 200.0f, points );
	float len = 0;
	for ( size_t i = 1; i < points.size(); i++ ) len += sqrtf( (points[i].x - points[i-1].x) * (points[i].x - points[i-1].x) + (points[i].z - points[i-1].z) * (points[i].z - points[i-1].z) );
	return len;
}

//
// Editing
//

static void spline_modified( void )
{
	g.projectmodified = 1;
}

static int spline_new( void )
{
	sSpline s;
	s.id = g_iSplineNextID++;
	sprintf_s( s.name, 64, "Spline %d", s.id );
	g_Splines.push_back( s );
	return (int)g_Splines.size() - 1;
}

// moves a node, and every node sharing its junction
static void spline_movenode( int si, int ni, float x, float y, float z )
{
	sSplineNode& node = g_Splines[ si ].nodes[ ni ];
	const int junction = node.junction;
	node.x = x; node.y = y; node.z = z;
	if ( junction == 0 ) return;
	for ( sSpline& s : g_Splines )
	{
		for ( sSplineNode& other : s.nodes )
		{
			if ( other.junction == junction ) { other.x = x; other.y = y; other.z = z; }
		}
	}
}

// a junction left with only one node is no junction
static void spline_cleanjunctions( void )
{
	std::vector<int> counts( g_iSplineNextJunction + 1, 0 );
	for ( sSpline& s : g_Splines )
		for ( sSplineNode& node : s.nodes )
			if ( node.junction > 0 && node.junction <= g_iSplineNextJunction ) counts[ node.junction ]++;
	for ( sSpline& s : g_Splines )
		for ( sSplineNode& node : s.nodes )
			if ( node.junction > 0 && (node.junction > g_iSplineNextJunction || counts[ node.junction ] < 2) ) node.junction = 0;
}

// joins two nodes (and the junctions they are already in) into one junction at the second node's position
static void spline_join( int si, int ni, int sj, int nj )
{
	sSplineNode& a = g_Splines[ si ].nodes[ ni ];
	sSplineNode& b = g_Splines[ sj ].nodes[ nj ];
	int junction = b.junction ? b.junction : (a.junction ? a.junction : g_iSplineNextJunction++);
	const int oldA = a.junction, oldB = b.junction;
	for ( sSpline& s : g_Splines )
	{
		for ( sSplineNode& node : s.nodes )
		{
			if ( (oldA && node.junction == oldA) || (oldB && node.junction == oldB) ) node.junction = junction;
		}
	}
	a.junction = junction;
	b.junction = junction;
	spline_movenode( sj, nj, b.x, b.y, b.z );
}

// a node at t on the segment; a Bezier segment is split so its shape stays
static int spline_insertnode( int si, int seg, float t )
{
	sSpline& s = g_Splines[ si ];
	const int n = (int)s.nodes.size();
	const int ib = spline_wrap( s, seg + 1 );
	float c[8];
	spline_controls( s, seg, c );
	sSplineNode node;
	spline_point( s, seg, t, &node.x, &node.z );
	node.y = spline_groundy( node.x, node.z );
	node.segCurve = s.nodes[ seg ].segCurve;
	node.segMark = s.nodes[ seg ].segMark;
	if ( spline_segcurve( s, seg ) == SPLINE_CURVE_BEZIER )
	{
		auto lerp = [t]( float a, float b ) { return a + (b - a) * t; };
		const float q0x = lerp( c[0], c[2] ), q0z = lerp( c[1], c[3] );
		const float q1x = lerp( c[2], c[4] ), q1z = lerp( c[3], c[5] );
		const float q2x = lerp( c[4], c[6] ), q2z = lerp( c[5], c[7] );
		const float r0x = lerp( q0x, q1x ), r0z = lerp( q0z, q1z );
		const float r1x = lerp( q1x, q2x ), r1z = lerp( q1z, q2z );
		s.nodes[ seg ].outX = q0x - c[0]; s.nodes[ seg ].outZ = q0z - c[1];
		s.nodes[ ib ].inX = q2x - c[6]; s.nodes[ ib ].inZ = q2z - c[7];
		node.inX = r0x - node.x; node.inZ = r0z - node.z;
		node.outX = r1x - node.x; node.outZ = r1z - node.z;
	}
	const int at = (seg + 1 >= n) ? n : seg + 1;
	s.nodes.insert( s.nodes.begin() + at, node );
	spline_modified();
	return at;
}

// a segment split into pieces nodes apart; returns the index of the segment after it
static int spline_subdividesegment( int si, int seg, int pieces )
{
	for ( int k = 0; k < pieces - 1; k++ )
	{
		// the rest of the segment from the last split is split at its next share
		seg = spline_insertnode( si, seg, 1.0f / (float)(pieces - k) );
	}
	return seg + 1;
}

static void spline_deletenode( int si, int ni )
{
	sSpline& s = g_Splines[ si ];
	s.nodes.erase( s.nodes.begin() + ni );
	if ( s.nodes.size() < 3 ) s.closed = 0;
	spline_cleanjunctions();
	if ( g_iSplineSelected == si ) { g_iSplineNodeSelected = -1; g_iSplineSegSelected = -1; }
	spline_modified();
}

static void spline_unbake( int si );
static void spline_rebuildwatermap( void );
static void spline_unplace( sSpline& s );

static void spline_deletespline( int si )
{
	spline_unplace( g_Splines[ si ] );
	spline_unbake( si );
	if ( g_Splines[ si ].waterEntity ) WickedCall_DeleteWaterSurface( g_Splines[ si ].waterEntity );
	g_Splines[ si ].waterTexels.clear();
	g_Splines.erase( g_Splines.begin() + si );
	spline_rebuildwatermap();
	spline_cleanjunctions();
	g_iSplineSelected = -1;
	g_iSplineNodeSelected = -1;
	g_iSplineSegSelected = -1;
	spline_modified();
}

static void spline_reverse( sSpline& s )
{
	const int n = (int)s.nodes.size();
	std::vector<int> segCurves( n ), segMarks( n );
	for ( int i = 0; i < n; i++ ) segCurves[ i ] = s.nodes[ i ].segCurve;
	for ( int i = 0; i < n; i++ ) segMarks[ i ] = s.nodes[ i ].segMark;
	std::reverse( s.nodes.begin(), s.nodes.end() );
	for ( int j = 0; j < n; j++ )
	{
		sSplineNode& node = s.nodes[ j ];
		std::swap( node.inX, node.outX );
		std::swap( node.inZ, node.outZ );
		// the segment from node j to the next was the one from old node n - 2 - j
		node.segCurve = segCurves[ spline_wrap( s, n - 2 - j ) ];
		node.segMark = segMarks[ spline_wrap( s, n - 2 - j ) ];
	}
}

// Bezier handles from the smooth curve, or along the segments for a straight one, so switching to Bezier keeps the shape
static void spline_seedhandles( sSpline& s, int fromCurve )
{
	const int n = (int)s.nodes.size();
	std::vector<float> handles( n * 4, 0.0f );
	sSpline from = s;
	from.curve = fromCurve;
	for ( int i = 0; i < n; i++ )
	{
		if ( fromCurve == SPLINE_CURVE_SMOOTH )
		{
			spline_handles( from, i, &handles[i*4+0], &handles[i*4+1], &handles[i*4+2], &handles[i*4+3] );
		}
		else
		{
			const bool bWrap = s.closed && n > 2;
			if ( i > 0 || bWrap ) { const sSplineNode& p = s.nodes[ spline_wrap( s, i - 1 ) ]; handles[i*4+0] = (p.x - s.nodes[i].x) / 3.0f; handles[i*4+1] = (p.z - s.nodes[i].z) / 3.0f; }
			if ( i < n - 1 || bWrap ) { const sSplineNode& p = s.nodes[ spline_wrap( s, i + 1 ) ]; handles[i*4+2] = (p.x - s.nodes[i].x) / 3.0f; handles[i*4+3] = (p.z - s.nodes[i].z) / 3.0f; }
		}
	}
	for ( int i = 0; i < n; i++ )
	{
		s.nodes[i].inX = handles[i*4+0]; s.nodes[i].inZ = handles[i*4+1];
		s.nodes[i].outX = handles[i*4+2]; s.nodes[i].outZ = handles[i*4+3];
	}
}

// joins spline sj onto spline si at their ends ni and nj; bShared when the two ends are one point (a junction), else a new
// segment runs between them. The joined spline has si's curve, and no segment changes: sj's keep theirs (each that
// followed sj's curve given it as its own) and their handles, the segment on from a shared point too
static void spline_merge( int si, int ni, int sj, int nj, bool bShared )
{
	if ( si == sj ) return;
	spline_unplace( g_Splines[ si ] );
	spline_unplace( g_Splines[ sj ] );
	spline_unbake( si );
	spline_unbake( sj );
	sSpline& a = g_Splines[ si ];
	sSpline b = g_Splines[ sj ];
	if ( b.curve != a.curve )
	{
		for ( sSplineNode& node : b.nodes ) if ( node.segCurve < 0 ) node.segCurve = b.curve;
	}
	if ( ni == 0 && a.nodes.size() > 1 ) spline_reverse( a );
	if ( nj != 0 ) spline_reverse( b );
	size_t first = 0;
	if ( bShared && !b.nodes.empty() )
	{
		sSplineNode& joint = a.nodes.back();
		joint.outX = b.nodes[0].outX;
		joint.outZ = b.nodes[0].outZ;
		joint.segCurve = b.nodes[0].segCurve;
		joint.segMark = b.nodes[0].segMark;
		first = 1;
	}
	for ( size_t k = first; k < b.nodes.size(); k++ ) a.nodes.push_back( b.nodes[k] );
	if ( g_Splines[ sj ].waterEntity ) WickedCall_DeleteWaterSurface( g_Splines[ sj ].waterEntity );
	g_Splines.erase( g_Splines.begin() + sj );
	if ( si > sj ) si--;
	g_iSplineSelected = si;
	g_iSplineNodeSelected = -1;
	spline_cleanjunctions();
	spline_modified();
}

// a node at the end of the spline that is being drawn: its last node, or its first while that is selected
static int spline_addend( int si, float x, float y, float z )
{
	sSpline& s = g_Splines[ si ];
	sSplineNode node;
	node.x = x; node.y = y; node.z = z;
	int at = (int)s.nodes.size();
	if ( g_iSplineSelected == si && g_iSplineNodeSelected == 0 && s.nodes.size() > 1 ) at = 0;
	s.nodes.insert( s.nodes.begin() + at, node );
	if ( s.curve == SPLINE_CURVE_BEZIER )
	{
		// handles for the new node a third of the way to its neighbour
		sSplineNode& added = s.nodes[ at ];
		const sSplineNode& next = s.nodes[ at == 0 ? (s.nodes.size() > 1 ? 1 : 0) : at - 1 ];
		const float dx = (next.x - added.x) / 3.0f, dz = (next.z - added.z) / 3.0f;
		if ( at == 0 ) { added.outX = dx; added.outZ = dz; added.inX = -dx; added.inZ = -dz; }
		else { added.inX = dx; added.inZ = dz; added.outX = -dx; added.outZ = -dz; }
	}
	spline_modified();
	return at;
}

//
// The bake
//

// what a bake depends on: the spline's kind, shape, junctions and road settings
static uint64_t spline_signature( const sSpline& s )
{
	uint64_t h = 0xcbf29ce484222325ULL;
	auto mix = [&h]( const void* p, size_t bytes ) { const uint8_t* b = (const uint8_t*)p; for ( size_t i = 0; i < bytes; i++ ) { h ^= b[i]; h *= 0x100000001b3ULL; } };
	mix( &s.kind, sizeof(s.kind) );
	mix( &s.curve, sizeof(s.curve) );
	mix( &s.closed, sizeof(s.closed) );
	for ( const sSplineNode& node : s.nodes )
	{
		const float f[6] = { node.x, node.z, node.inX, node.inZ, node.outX, node.outZ };
		mix( f, sizeof(f) );
		mix( &node.junction, sizeof(node.junction) );
		mix( &node.segCurve, sizeof(node.segCurve) );
	}
	const sSplineRoad& r = s.road;
	const float rf[7] = { r.width, r.shoulder, r.smoothing, r.maxGrade, r.crown, r.grassMargin, r.treeMargin };
	const int ri[2] = { r.material, r.edgeMaterial };
	mix( rf, sizeof(rf) );
	mix( ri, sizeof(ri) );
	const sSplineRiver& v = s.river;
	const float vf[6] = { v.bedWidth, v.depth, v.banks, v.smoothing, v.grassMargin, v.treeMargin };
	const int vi[3] = { v.bedMaterial, v.bankMaterial, v.downhill };
	mix( vf, sizeof(vf) );
	mix( vi, sizeof(vi) );
	if ( s.kind == SPLINE_KIND_RIVER )
	{
		mix( &v.raiseBanks, sizeof(v.raiseBanks) );
		if ( v.raiseBanks ) mix( &v.waterDepth, sizeof(v.waterDepth) );
	}
	return h ? h : 1;
}

static void spline_texelbounds( int x, int z, float* pBounds )
{
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	const float wx = ((float)x / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
	const float wz = ((float)z / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
	if ( wx < pBounds[0] ) pBounds[0] = wx;
	if ( wz < pBounds[1] ) pBounds[1] = wz;
	if ( wx > pBounds[2] ) pBounds[2] = wx;
	if ( wz > pBounds[3] ) pBounds[3] = wz;
}

// puts back what the spline's bake changed, where the bake's value is still there
static void spline_restore( sSpline& s, float* pBounds )
{
	float* pH = GGTerrain::GGTerrain_GetHeightEditMap();
	uint8_t* pT = GGTerrain::GGTerrain_GetHeightEditTypeMap();
	uint8_t* pM = GGTerrain::GGTerrain_GetMaterialMap();
	uint8_t* pG = GGGrass::GGGrass_GetGrassMap();
	for ( const sSplineBakeTexel& b : s.baked )
	{
		if ( b.x >= SPLINE_MAP_SIZE || b.z >= SPLINE_MAP_SIZE ) continue;
		const uint32_t hIndex = (SPLINE_MAP_SIZE - 1 - b.z) * SPLINE_MAP_SIZE + b.x;
		const uint32_t mIndex = b.z * SPLINE_MAP_SIZE + b.x;
		if ( (b.flags & SPLINE_TEXEL_HEIGHT) && pH && pT && pH[ hIndex ] == b.heightAfter && pT[ hIndex ] == b.typeAfter )
		{
			pH[ hIndex ] = b.heightBefore;
			pT[ hIndex ] = b.typeBefore;
		}
		if ( (b.flags & SPLINE_TEXEL_MATERIAL) && pM && pM[ mIndex ] == b.matAfter ) pM[ mIndex ] = b.matBefore;
		if ( (b.flags & SPLINE_TEXEL_GRASS) && pG && pG[ mIndex ] == b.grassAfter ) pG[ mIndex ] = b.grassBefore;
		spline_texelbounds( b.x, b.z, pBounds );
	}
	for ( const sSplineBakeTree& tree : s.bakedTrees )
	{
		GGTrees::GGTrees_ShowTree( tree.id, tree.x, tree.z, tree.data );
		const int ix = (int)((tree.x / GGTerrain::GGTerrain_GetEditableSize() * 0.5f + 0.5f) * SPLINE_MAP_SIZE);
		const int iz = (int)((tree.z / GGTerrain::GGTerrain_GetEditableSize() * 0.5f + 0.5f) * SPLINE_MAP_SIZE);
		spline_texelbounds( ix, iz, pBounds );
	}
	s.baked.clear();
	s.bakedTrees.clear();
}

struct sRoadSample
{
	float x, z, s, ground, h;
};

struct sRoadFoot
{
	float d, h; // at the texel's corner, where its height is (the height edit map's grid)
	int k;
	float dc; // at its centre, where its paint and grass are shown (the material and grass maps are read per texel)
};

// the centre line about spacing apart with the ground along it (an earlier road's surface included), and the sample at
// each node; false when there is no line
static bool spline_centreline( const sSpline& sp, float spacing, std::vector<sRoadSample>& c, std::vector<int>& nodeSample )
{
	c.clear();
	nodeSample.assign( sp.nodes.size(), -1 );
	const int segs = spline_segments( sp );
	if ( segs == 0 ) return false;
	for ( int seg = 0; seg < segs; seg++ )
	{
		float ctl[8];
		spline_controls( sp, seg, ctl );
		float polygon = 0;
		for ( int k = 0; k < 3; k++ ) polygon += sqrtf( (ctl[k*2+2] - ctl[k*2]) * (ctl[k*2+2] - ctl[k*2]) + (ctl[k*2+3] - ctl[k*2+1]) * (ctl[k*2+3] - ctl[k*2+1]) );
		int steps = (int)(polygon / spacing) + 1;
		if ( steps > 8192 ) steps = 8192;
		nodeSample[ seg ] = (int)c.size();
		for ( int k = 0; k < steps; k++ )
		{
			sRoadSample p;
			spline_point( sp, seg, (float)k / steps, &p.x, &p.z );
			c.push_back( p );
		}
	}
	sRoadSample last;
	spline_point( sp, segs - 1, 1.0f, &last.x, &last.z );
	const int lastNode = spline_wrap( sp, segs );
	if ( nodeSample[ lastNode ] < 0 ) nodeSample[ lastNode ] = (int)c.size();
	c.push_back( last );
	const int n = (int)c.size();
	if ( n < 2 ) return false;
	c[0].s = 0;
	for ( int i = 1; i < n; i++ ) c[i].s = c[i-1].s + sqrtf( (c[i].x - c[i-1].x) * (c[i].x - c[i-1].x) + (c[i].z - c[i-1].z) * (c[i].z - c[i-1].z) );
	for ( sRoadSample& p : c )
	{
		if ( !GGTerrain::GGTerrain_GetHeight( p.x, p.z, &p.ground, 1, 1 ) || p.ground != p.ground ) p.ground = spline_groundy( p.x, p.z );
	}
	return true;
}

// the ground averaged over a length into the profile
static void spline_smoothprofile( std::vector<sRoadSample>& c, float length )
{
	const int n = (int)c.size();
	std::vector<double> prefix( n + 1, 0.0 );
	for ( int i = 0; i < n; i++ ) prefix[ i + 1 ] = prefix[ i ] + c[i].ground;
	const float avgSpacing = c[ n - 1 ].s / (float)(n - 1);
	const int window = avgSpacing > 0 ? (int)(length * 0.5f / avgSpacing) : 0;
	for ( int i = 0; i < n; i++ )
	{
		const int a = std::max( 0, i - window ), b = std::min( n - 1, i + window );
		c[i].h = (float)((prefix[ b + 1 ] - prefix[ a ]) / (double)(b - a + 1));
	}
}

// the profile held at the pins (sample, height) and shifted linearly between them
static void spline_pinprofile( std::vector<sRoadSample>& c, std::vector<std::pair<int, float>>& pins, std::vector<char>& pinned )
{
	const int n = (int)c.size();
	pinned.assign( n, 0 );
	std::sort( pins.begin(), pins.end() );
	pins.erase( std::unique( pins.begin(), pins.end(), []( const std::pair<int, float>& a, const std::pair<int, float>& b ) { return a.first == b.first; } ), pins.end() );
	if ( pins.empty() ) return;
	std::vector<float> corr( n, 0.0f );
	size_t pi = 0;
	for ( int i = 0; i < n; i++ )
	{
		while ( pi + 1 < pins.size() && pins[ pi + 1 ].first <= i ) pi++;
		const int ia = pins[ pi ].first;
		const float ca = pins[ pi ].second - c[ ia ].h;
		if ( i <= ia || pi + 1 >= pins.size() ) { corr[i] = ca; continue; }
		const int ib = pins[ pi + 1 ].first;
		const float cb = pins[ pi + 1 ].second - c[ ib ].h;
		const float span = c[ ib ].s - c[ ia ].s;
		corr[i] = span > 0 ? ca + (cb - ca) * (c[i].s - c[ ia ].s) / span : ca;
	}
	for ( int i = 0; i < n; i++ ) c[i].h += corr[i];
	for ( const auto& pin : pins ) { c[ pin.first ].h = pin.second; pinned[ pin.first ] = 1; }
}

// the shape across a spline: a core (a carriageway, a river bed) flat at the profile, edges (shoulders, banks) blended
// from it to the ground, textures on each, grass cleared and trees hidden within margins past the core's edge
struct sSplineShape
{
	float halfCore = 0, edge = 0, crown = 0;
	int coreMaterial = 0, edgeMaterial = 0;
	float grassMargin = 0, treeMargin = 0;
	float crest = 0; // above 0, the edges rise at least this far above the core before falling to lower ground (a river's raised banks)
};

// the footprint on the terrain's texels, and the writes
static void spline_bakeshape( sSpline& sp, const std::vector<sRoadSample>& c, const sSplineShape& shape, std::unordered_set<uint32_t>& protectedTexels, float* pBounds )
{
	float* pH = GGTerrain::GGTerrain_GetHeightEditMap();
	uint8_t* pT = GGTerrain::GGTerrain_GetHeightEditTypeMap();
	uint8_t* pM = GGTerrain::GGTerrain_GetMaterialMap();
	uint8_t* pG = GGGrass::GGGrass_GetGrassMap();
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	const int n = (int)c.size();
	if ( !pH || !pT || E <= 0 || n < 2 || shape.halfCore <= 0 ) return;
	const float texel = E * 2.0f / SPLINE_MAP_SIZE;
	const float halfW = shape.halfCore;

	// every texel near the centre line (in one texel pieces), its distance and the profile there
	const float reach = halfW + std::max( std::max( shape.edge, shape.grassMargin ), shape.treeMargin );
	const float spacing = c[ n - 1 ].s / (float)(n - 1);
	const int step = std::max( 1, (int)(texel / std::max( 1.0f, spacing ) + 0.5f) );
	std::unordered_map<uint32_t, sRoadFoot> foot;
	foot.reserve( (size_t)(c[ n - 1 ].s / texel * (reach * 2.0f / texel + 2.0f)) + 64 );
	auto toTexel = [E]( float v ) { return (v / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE; };
	for ( int k = 0; k < n - 1; k += step )
	{
		const int k2 = std::min( n - 1, k + step );
		const sRoadSample& a = c[ k ];
		const sRoadSample& b = c[ k2 ];
		int ix0 = (int)floorf( toTexel( std::min( a.x, b.x ) - reach ) ), ix1 = (int)ceilf( toTexel( std::max( a.x, b.x ) + reach ) );
		int iz0 = (int)floorf( toTexel( std::min( a.z, b.z ) - reach ) ), iz1 = (int)ceilf( toTexel( std::max( a.z, b.z ) + reach ) );
		ix0 = std::max( 1, ix0 ); iz0 = std::max( 1, iz0 );
		ix1 = std::min( SPLINE_MAP_SIZE - 2, ix1 ); iz1 = std::min( SPLINE_MAP_SIZE - 2, iz1 );
		const float dx = b.x - a.x, dz = b.z - a.z;
		const float len2 = dx * dx + dz * dz;
		for ( int iz = iz0; iz <= iz1; iz++ )
		{
			const float wz = ((float)iz / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
			for ( int ix = ix0; ix <= ix1; ix++ )
			{
				const float wx = ((float)ix / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
				float t = len2 > 0 ? ((wx - a.x) * dx + (wz - a.z) * dz) / len2 : 0;
				t = std::min( 1.0f, std::max( 0.0f, t ) );
				const float px = a.x + dx * t - wx, pz = a.z + dz * t - wz;
				const float dist = sqrtf( px * px + pz * pz );
				// the texel's centre, half a texel on: the terrain shows its paint and grass there, so measured by the corner
				// a road's paint sat half a texel (a metre on an 8 km level) off its line, and off its placed lamps
				const float wxc = wx + texel * 0.5f, wzc = wz + texel * 0.5f;
				float tc = len2 > 0 ? ((wxc - a.x) * dx + (wzc - a.z) * dz) / len2 : 0;
				tc = std::min( 1.0f, std::max( 0.0f, tc ) );
				const float pxc = a.x + dx * tc - wxc, pzc = a.z + dz * tc - wzc;
				const float distc = sqrtf( pxc * pxc + pzc * pzc );
				if ( dist > reach && distc > reach ) continue;
				const uint32_t key = (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix;
				auto it = foot.find( key );
				if ( it == foot.end() ) foot[ key ] = { dist, a.h + (b.h - a.h) * t, k, distc };
				else
				{
					if ( dist < it->second.d ) { it->second.d = dist; it->second.h = a.h + (b.h - a.h) * t; it->second.k = k; }
					if ( distc < it->second.dc ) it->second.dc = distc;
				}
			}
		}
	}

	// the core flat at the profile, the edges blended to the ground (not over an earlier road's carriageway), the
	// textures, the grass cleared
	std::vector<uint32_t> core;
	for ( const auto& entry : foot )
	{
		const uint32_t key = entry.first;
		const sRoadFoot& f = entry.second;
		const int ix = (int)(key % SPLINE_MAP_SIZE), iz = (int)(key / SPLINE_MAP_SIZE);
		const float wx = ((float)ix / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
		const float wz = ((float)iz / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
		const uint32_t hIndex = (SPLINE_MAP_SIZE - 1 - iz) * SPLINE_MAP_SIZE + ix;
		const uint32_t mIndex = key;
		const bool bCore = f.d <= halfW;
		const bool bProtected = !bCore && protectedTexels.count( key ) > 0;
		sSplineBakeTexel b;
		b.x = (uint16_t)ix;
		b.z = (uint16_t)iz;
		b.heightBefore = pH[ hIndex ];
		b.typeBefore = pT[ hIndex ];
		b.matBefore = pM ? pM[ mIndex ] : 0;
		b.grassBefore = pG ? pG[ mIndex ] : 0;
		if ( bCore )
		{
			const float e = f.d / halfW;
			pH[ hIndex ] = GGTerrain::GGTerrain_HeightToEdit( f.h - shape.crown * e * e );
			pT[ hIndex ] = 1;
			b.flags |= SPLINE_TEXEL_CARRIAGEWAY | SPLINE_TEXEL_HEIGHT;
			core.push_back( key );
		}
		else if ( f.d <= halfW + shape.edge && !bProtected && shape.edge > 0 )
		{
			float ground = f.h;
			if ( !GGTerrain::GGTerrain_GetHeight( wx, wz, &ground, 1, 1 ) || ground != ground ) ground = f.h;
			const float tt = (f.d - halfW) / shape.edge;
			const float smooth = tt * tt * (3.0f - 2.0f * tt);
			float target = f.h + (ground - f.h) * smooth;
			const float crest = f.h + shape.crest;
			if ( shape.crest > 0 && ground < crest )
			{
				// a raised bank: up from the core to the crest over the first half of the edge, down to the ground over the rest
				const float u = tt < 0.5f ? tt * 2.0f : (tt - 0.5f) * 2.0f;
				const float s = u * u * (3.0f - 2.0f * u);
				target = tt < 0.5f ? f.h + (crest - f.h) * s : crest + (ground - crest) * s;
			}
			pH[ hIndex ] = GGTerrain::GGTerrain_HeightToEdit( target );
			pT[ hIndex ] = 1;
			b.flags |= SPLINE_TEXEL_HEIGHT;
		}
		// paint and grass by the texel's centre
		const bool bPaintCore = f.dc <= halfW;
		const bool bPaintProtected = !bPaintCore && protectedTexels.count( key ) > 0;
		if ( pM )
		{
			if ( bPaintCore && shape.coreMaterial > 0 ) { pM[ mIndex ] = (uint8_t)shape.coreMaterial; b.flags |= SPLINE_TEXEL_MATERIAL; }
			else if ( !bPaintCore && !bPaintProtected && shape.edgeMaterial > 0 && f.dc <= halfW + shape.edge ) { pM[ mIndex ] = (uint8_t)shape.edgeMaterial; b.flags |= SPLINE_TEXEL_MATERIAL; }
		}
		if ( pG && f.dc <= halfW + shape.grassMargin && !bPaintProtected )
		{
			pG[ mIndex ] &= 0x80;
			b.flags |= SPLINE_TEXEL_GRASS;
		}
		if ( b.flags & (SPLINE_TEXEL_HEIGHT | SPLINE_TEXEL_MATERIAL | SPLINE_TEXEL_GRASS) )
		{
			b.heightAfter = pH[ hIndex ];
			b.typeAfter = pT[ hIndex ];
			b.matAfter = pM ? pM[ mIndex ] : 0;
			b.grassAfter = pG ? pG[ mIndex ] : 0;
			sp.baked.push_back( b );
			spline_texelbounds( ix, iz, pBounds );
		}
	}
	for ( uint32_t key : core ) protectedTexels.insert( key );

	// the trees whose trunks stand within the tree margin of the core
	float minX = FLT_MAX, minZ = FLT_MAX, maxX = -FLT_MAX, maxZ = -FLT_MAX;
	for ( const sRoadSample& p : c ) { minX = std::min( minX, p.x ); minZ = std::min( minZ, p.z ); maxX = std::max( maxX, p.x ); maxZ = std::max( maxZ, p.z ); }
	const float treeReach = halfW + shape.treeMargin;
	std::vector<GGTrees::GGTreeSlot> trees;
	GGTrees::GGTrees_GetTreesInRect( minX - treeReach - 200.0f, minZ - treeReach - 200.0f, maxX + treeReach + 200.0f, maxZ + treeReach + 200.0f, trees );
	for ( const GGTrees::GGTreeSlot& tree : trees )
	{
		const int ix = (int)(toTexel( tree.trunkX ) + 0.5f), iz = (int)(toTexel( tree.trunkZ ) + 0.5f);
		if ( ix < 0 || iz < 0 || ix >= SPLINE_MAP_SIZE || iz >= SPLINE_MAP_SIZE ) continue;
		auto it = foot.find( (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix );
		if ( it == foot.end() ) continue;
		// the exact distance to the centre line near that texel's nearest piece
		float best = FLT_MAX;
		for ( int k = std::max( 0, it->second.k - step * 2 ); k < std::min( n - 1, it->second.k + step * 3 ); k++ )
		{
			const float dx = c[k+1].x - c[k].x, dz = c[k+1].z - c[k].z;
			const float len2 = dx * dx + dz * dz;
			float t = len2 > 0 ? ((tree.trunkX - c[k].x) * dx + (tree.trunkZ - c[k].z) * dz) / len2 : 0;
			t = std::min( 1.0f, std::max( 0.0f, t ) );
			const float px = c[k].x + dx * t - tree.trunkX, pz = c[k].z + dz * t - tree.trunkZ;
			best = std::min( best, sqrtf( px * px + pz * pz ) );
		}
		if ( best > treeReach + tree.diameter * 0.5f ) continue;
		GGTrees::GGTrees_HideTree( tree.id );
		sSplineBakeTree record;
		record.id = tree.id;
		record.x = tree.x;
		record.z = tree.z;
		record.data = tree.data;
		sp.bakedTrees.push_back( record );
		spline_texelbounds( ix, iz, pBounds );
	}
}

// a road: the ground averaged, pinned to the ground at open ends and junctions (so it meets an earlier road's surface),
// held to the maximum grade; flat across the carriageway with an optional crown
static void spline_bakeroad( sSpline& sp, std::unordered_set<uint32_t>& protectedTexels, float* pBounds )
{
	const sSplineRoad& r = sp.road;
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( E <= 0 || r.width <= 0 ) return;
	std::vector<sRoadSample> c;
	std::vector<int> nodeSample;
	if ( !spline_centreline( sp, E * 2.0f / SPLINE_MAP_SIZE * 0.25f, c, nodeSample ) ) return;
	const int n = (int)c.size();
	spline_smoothprofile( c, r.smoothing );
	std::vector<std::pair<int, float>> pins;
	if ( !(sp.closed && sp.nodes.size() > 2) )
	{
		pins.push_back( { 0, c[0].ground } );
		pins.push_back( { n - 1, c[ n - 1 ].ground } );
	}
	for ( size_t ni = 0; ni < sp.nodes.size(); ni++ )
	{
		if ( sp.nodes[ ni ].junction && nodeSample[ ni ] >= 0 ) pins.push_back( { nodeSample[ ni ], c[ nodeSample[ ni ] ].ground } );
	}
	std::vector<char> pinned;
	spline_pinprofile( c, pins, pinned );

	// no steeper than the maximum grade, both ways, pins kept
	const float grade = std::max( 0.001f, r.maxGrade / 100.0f );
	for ( int pass = 0; pass < 2; pass++ )
	{
		for ( int i = 1; i < n; i++ )
		{
			if ( pinned[i] ) continue;
			const float rise = grade * (c[i].s - c[i-1].s);
			c[i].h = std::min( std::max( c[i].h, c[i-1].h - rise ), c[i-1].h + rise );
		}
		for ( int i = n - 2; i >= 0; i-- )
		{
			if ( pinned[i] ) continue;
			const float rise = grade * (c[i+1].s - c[i].s);
			c[i].h = std::min( std::max( c[i].h, c[i+1].h - rise ), c[i+1].h + rise );
		}
	}

	sSplineShape shape;
	shape.halfCore = r.width * 0.5f;
	shape.edge = r.shoulder;
	shape.crown = r.crown;
	shape.coreMaterial = r.material;
	shape.edgeMaterial = r.edgeMaterial;
	shape.grassMargin = r.grassMargin;
	shape.treeMargin = r.treeMargin;
	spline_bakeshape( sp, c, shape, protectedTexels, pBounds );
}

// a river: the ground averaged and lowered by the depth, never rising from the first node to the last when downhill;
// pinned at a junction with an earlier river (its bed there), so a tributary meets it; a flat bed, banks up to the ground
static void spline_bakeriver( int si, std::unordered_set<uint32_t>& protectedTexels, float* pBounds )
{
	sSpline& sp = g_Splines[ si ];
	const sSplineRiver& v = sp.river;
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( E <= 0 || v.bedWidth <= 0 ) return;
	std::vector<sRoadSample> c;
	std::vector<int> nodeSample;
	if ( !spline_centreline( sp, E * 2.0f / SPLINE_MAP_SIZE * 0.25f, c, nodeSample ) ) return;
	const int n = (int)c.size();
	spline_smoothprofile( c, v.smoothing );
	for ( sRoadSample& p : c ) p.h -= v.depth;
	std::vector<std::pair<int, float>> pins;
	for ( size_t ni = 0; ni < sp.nodes.size(); ni++ )
	{
		const int junction = sp.nodes[ ni ].junction;
		if ( !junction || nodeSample[ ni ] < 0 ) continue;
		bool bEarlierRiver = false;
		for ( int sj = 0; sj < si && !bEarlierRiver; sj++ )
		{
			if ( g_Splines[ sj ].kind != SPLINE_KIND_RIVER ) continue;
			for ( const sSplineNode& other : g_Splines[ sj ].nodes ) if ( other.junction == junction ) { bEarlierRiver = true; break; }
		}
		if ( bEarlierRiver ) pins.push_back( { nodeSample[ ni ], c[ nodeSample[ ni ] ].ground } );
	}
	std::vector<char> pinned;
	spline_pinprofile( c, pins, pinned );
	if ( v.downhill )
	{
		for ( int i = 1; i < n; i++ ) if ( !pinned[i] ) c[i].h = std::min( c[i].h, c[i-1].h );
	}

	// how much of the bed is below the level's water line (only that part holds water)
	int wet = 0;
	for ( const sRoadSample& p : c ) if ( p.h < t.terrain.waterliney_f ) wet++;
	sp.wetFraction = (float)wet / (float)n;

	sSplineShape shape;
	shape.halfCore = v.bedWidth * 0.5f;
	shape.edge = v.banks;
	if ( v.raiseBanks && v.waterDepth > 0 ) shape.crest = v.waterDepth + 40.0f; // a metre above the water
	shape.coreMaterial = v.bedMaterial;
	shape.edgeMaterial = v.bankMaterial;
	shape.grassMargin = v.grassMargin;
	shape.treeMargin = v.treeMargin;
	spline_bakeshape( sp, c, shape, protectedTexels, pBounds );
}

// makes the terrain, grass and trees show the changes inside the bounds
static void spline_refresh( const float* pBounds )
{
	if ( pBounds[0] > pBounds[2] ) return;
	const float margin = GGTerrain::GGTerrain_GetEditableSize() * 4.0f / SPLINE_MAP_SIZE;
	GGTerrain::GGTerrain_InvalidateRegion( pBounds[0] - margin, pBounds[1] - margin, pBounds[2] + margin, pBounds[3] + margin, GGTERRAIN_INVALIDATE_ALL );
	GGTerrain::GGTerrain_MaterialMapChanged( pBounds[0] - margin, pBounds[1] - margin, pBounds[2] + margin, pBounds[3] + margin );
	GGGrass::GGGrass_UpdateInstances();
	GGTrees::GGTrees_RefreshRect( pBounds[0] - margin, pBounds[1] - margin, pBounds[2] + margin, pBounds[3] + margin );
	g.projectmodified = 1;
}

// the splines joined to the given ones, through their junctions
static void spline_joined( std::vector<char>& in )
{
	bool bGrew = true;
	while ( bGrew )
	{
		bGrew = false;
		std::unordered_set<int> junctions;
		for ( size_t si = 0; si < g_Splines.size(); si++ )
			if ( in[ si ] ) for ( const sSplineNode& node : g_Splines[ si ].nodes ) if ( node.junction ) junctions.insert( node.junction );
		for ( size_t si = 0; si < g_Splines.size(); si++ )
		{
			if ( in[ si ] ) continue;
			for ( const sSplineNode& node : g_Splines[ si ].nodes )
			{
				if ( node.junction && junctions.count( node.junction ) ) { in[ si ] = 1; bGrew = true; break; }
			}
		}
	}
}

// restores the splines in the group (last first) and bakes the roads among them (in list order)
static void spline_bakegroup( const std::vector<char>& in )
{
	float bounds[4] = { FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX };
	for ( int si = (int)g_Splines.size() - 1; si >= 0; si-- ) if ( in[ si ] ) spline_restore( g_Splines[ si ], bounds );

	// the other roads' carriageways, where their bake is still there, keep their surface
	float* pH = GGTerrain::GGTerrain_GetHeightEditMap();
	std::unordered_set<uint32_t> protectedTexels;
	for ( size_t si = 0; si < g_Splines.size(); si++ )
	{
		if ( in[ si ] ) continue;
		for ( const sSplineBakeTexel& b : g_Splines[ si ].baked )
		{
			if ( !(b.flags & SPLINE_TEXEL_CARRIAGEWAY) || !pH ) continue;
			if ( pH[ (SPLINE_MAP_SIZE - 1 - b.z) * SPLINE_MAP_SIZE + b.x ] == b.heightAfter ) protectedTexels.insert( (uint32_t)b.z * SPLINE_MAP_SIZE + b.x );
		}
	}

	for ( size_t si = 0; si < g_Splines.size(); si++ )
	{
		if ( !in[ si ] ) continue;
		sSpline& s = g_Splines[ si ];
		if ( s.kind == SPLINE_KIND_ROAD ) spline_bakeroad( s, protectedTexels, bounds );
		else if ( s.kind == SPLINE_KIND_RIVER ) spline_bakeriver( (int)si, protectedTexels, bounds );
		s.bakedSignature = spline_signature( s );
		s.bakeCount++;
	}
	spline_refresh( bounds );
}

// restores one spline's bake now (before it is deleted or merged), and marks the splines joined to it to bake again
static void spline_unbake( int si )
{
	sSpline& s = g_Splines[ si ];
	if ( s.baked.empty() && s.bakedTrees.empty() ) return;
	float bounds[4] = { FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX };
	spline_restore( s, bounds );
	spline_refresh( bounds );
	std::vector<char> in( g_Splines.size(), 0 );
	in[ si ] = 1;
	spline_joined( in );
	for ( size_t sj = 0; sj < g_Splines.size(); sj++ ) if ( in[ sj ] && (int)sj != si ) g_Splines[ sj ].bakedSignature = 0;
}

// bakes the splines that changed since their last bake (roads with Auto Apply, and Apply), with every spline joined to
// them; never while dragging or editing a value, nor while the terrain is regenerating
static void spline_bakechanged( void )
{
	if ( iDragNode >= 0 || ImGui::IsAnyItemActive() ) return;
	if ( ImGui::GetFrameCount() - g_iSplineLoadFrame < SPLINE_LOAD_SETTLE_FRAMES ) return;
	if ( !GGTerrain::GGTerrain_IsReady() ) return;
	const size_t count = g_Splines.size();
	std::vector<char> in( count, 0 );
	bool bAny = false;
	for ( size_t si = 0; si < count; si++ )
	{
		const sSpline& s = g_Splines[ si ];
		const bool bChanged = spline_signature( s ) != s.bakedSignature;
		bool bBake = false;
		const bool bForce = (int)si == iApplySpline || g_SplineForceBake.count( s.id ) != 0;
		if ( s.kind == SPLINE_KIND_ROAD ) bBake = (bChanged && s.road.autoApply) || bForce;
		else if ( s.kind == SPLINE_KIND_RIVER ) bBake = (bChanged && s.river.autoApply) || bForce;
		else bBake = !s.baked.empty() || !s.bakedTrees.empty();
		if ( bBake ) { in[ si ] = 1; bAny = true; }
	}
	iApplySpline = -1;
	g_SplineForceBake.clear();
	if ( !bAny ) return;
	spline_joined( in );
	spline_bakegroup( in );
}

//
// River water
//

static uint64_t spline_hashmix( uint64_t h, const void* p, size_t bytes )
{
	const uint8_t* b = (const uint8_t*)p;
	for ( size_t i = 0; i < bytes; i++ ) { h ^= b[i]; h *= 0x100000001b3ULL; }
	return h;
}

// the river's water look: the main water's colour, with its flow and waves relative to their defaults, or its own
static WickedCallWaterLook spline_waterlook( const sSplineRiver& v )
{
	WickedCallWaterLook look;
	if ( v.mainLook )
	{
		look.r = t.visuals.WaterRed_f / 255.0f;
		look.g = t.visuals.WaterGreen_f / 255.0f;
		look.b = t.visuals.WaterBlue_f / 255.0f;
		look.a = 0.8f;
		look.speed = std::min( 4.0f, std::max( 0.1f, t.visuals.WaterSpeed1 / 0.06f ) );
		look.distortion = std::min( 4.0f, std::max( 0.1f, t.visuals.fWaterWaveAmplitude / 20.0f ) );
		look.fogMin = t.visuals.WaterFogMinDist;
		look.fogMax = t.visuals.WaterFogMaxDist;
		look.fogMinAmount = t.visuals.WaterFogMinAmount;
		look.foam = v.foam;
	}
	else
	{
		look.r = v.colour[0]; look.g = v.colour[1]; look.b = v.colour[2];
		look.a = v.clarity;
		look.speed = v.flow;
		look.distortion = v.waves;
		look.foam = v.foam;
		look.uvScale = v.ripples > 0.01f ? 1.0f / v.ripples : 1.0f;
		look.fogMin = 0.0f;
		look.fogMax = v.seeDepth;
		look.fogMinAmount = 1.0f - v.clarity;
	}
	look.direction = 0.75f; // downstream, the surface's v rising (the shader's pattern moves against its direction)
	look.scroll = look.speed;
	return look;
}

static uint64_t spline_waterlookhash( const WickedCallWaterLook& look )
{
	return spline_hashmix( 0xcbf29ce484222325ULL, &look, sizeof(look) );
}

static void spline_rebuildwatermap( void )
{
	g_RiverWater.clear();
	for ( const sSpline& s : g_Splines )
	{
		for ( const auto& texel : s.waterTexels )
		{
			auto it = g_RiverWater.find( texel.first );
			if ( it == g_RiverWater.end() || texel.second.height > it->second.height ) g_RiverWater[ texel.first ] = texel.second;
		}
	}
}

// the river's water surface, a cross section a texel apart along the river:
// - its level the water depth above the baked bed, but no higher than the lower bank's crest (so the water never stands
//   over lower ground beside it), the dip eased along the river
// - each side reaching to where the terrain rises above the water, and a little further under the bank so its edge is
//   never seen
// - where it falls to the sea it meets the sea's surface and ends
// Its texture coordinates are in world units, so the ripples keep their size; and the texels under it are kept, with the
// water's height over each
static void spline_buildwater( sSpline& s )
{
	if ( s.waterEntity ) WickedCall_DeleteWaterSurface( s.waterEntity );
	s.waterEntity = 0;
	s.waterTexels.clear();
	s.waterLowered = 0.0f;
	if ( s.kind != SPLINE_KIND_RIVER || s.baked.empty() ) return;
	const sSplineRiver& v = s.river;
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( E <= 0 || v.waterDepth <= 0 ) return;
	const float texel = E * 2.0f / SPLINE_MAP_SIZE;
	std::vector<sRoadSample> c;
	std::vector<int> nodeSample;
	if ( !spline_centreline( s, texel, c, nodeSample ) ) return;
	const int n = (int)c.size();
	const float sea = t.terrain.waterliney_f;

	// each section's normal (left is -, right +) and the baked terrain out along each side
	const float step = texel * 0.5f;
	const float reach = v.bedWidth * 0.5f + v.banks + std::max( v.banks * 0.5f, texel * 2.0f );
	const int steps = std::max( 3, (int)(reach / step) + 2 );
	std::vector<float> nxs( n ), nzs( n ), sideH( (size_t)n * 2 * steps );
	std::vector<float> limit( n ), level( n );
	int lowered = 0;
	for ( int i = 0; i < n; i++ )
	{
		const sRoadSample& a = c[ std::max( 0, i - 1 ) ];
		const sRoadSample& b = c[ std::min( n - 1, i + 1 ) ];
		float tx = b.x - a.x, tz = b.z - a.z;
		const float len = sqrtf( tx * tx + tz * tz );
		if ( len > 0 ) { tx /= len; tz /= len; }
		nxs[i] = -tz;
		nzs[i] = tx;
		float crest[2] = { -FLT_MAX, -FLT_MAX };
		for ( int side = 0; side < 2; side++ )
		{
			const float sign = side == 0 ? -1.0f : 1.0f;
			float* h = &sideH[ ((size_t)i * 2 + side) * steps ];
			for ( int k = 0; k < steps; k++ )
			{
				const float px = c[i].x + sign * nxs[i] * k * step, pz = c[i].z + sign * nzs[i] * k * step;
				float y = 0;
				if ( !GGTerrain::GGTerrain_GetHeight( px, pz, &y, 1, 1 ) || y != y ) y = spline_groundy( px, pz );
				h[k] = y;
				crest[ side ] = std::max( crest[ side ], y );
			}
		}
		const float design = c[i].ground + v.waterDepth;
		limit[i] = std::min( design, std::min( crest[0], crest[1] ) - 10.0f );
		if ( limit[i] < design - 5.0f ) lowered++;
	}
	s.waterLowered = (float)lowered / (float)n;

	// how far a low bank lowers the water, eased over a few sections either side, never above any section's own limit;
	// the lowering is eased, not the level, which falls fast along a steep river
	const int window = 4;
	std::vector<float> lowering( n ), widened( n );
	for ( int i = 0; i < n; i++ ) lowering[i] = (c[i].ground + v.waterDepth) - limit[i];
	for ( int i = 0; i < n; i++ )
	{
		float m = lowering[i];
		for ( int j = std::max( 0, i - window ); j <= std::min( n - 1, i + window ); j++ ) m = std::max( m, lowering[j] );
		widened[i] = m;
	}
	for ( int i = 0; i < n; i++ )
	{
		float sum = 0;
		int count = 0;
		for ( int j = std::max( 0, i - window ); j <= std::min( n - 1, i + window ); j++ ) { sum += widened[j]; count++; }
		level[i] = std::min( c[i].ground + v.waterDepth - sum / count, limit[i] );
	}

	// the water's slope (downhill positive), its turbulence where the slope changes sharply (the lip and the foot of a steep
	// run) and a little along a steep run, eased out up and down stream; and how fast it runs, faster where it is steep
	// the slope is taken from the level smoothed over some 20 m, as the baked heights step a little from texel to texel, and
	// only a change of slope over 3% (or a slope over 12%) counts, so an even slope has none
	std::vector<float> grade( n ), turbulence( n ), speed( n ), smoothLevel( n );
	for ( int i = 0; i < n; i++ )
	{
		float sum = 0;
		int count = 0;
		for ( int j = std::max( 0, i - 6 ); j <= std::min( n - 1, i + 6 ); j++ ) { sum += level[j]; count++; }
		smoothLevel[i] = sum / count;
	}
	for ( int i = 0; i < n; i++ )
	{
		const int a = std::max( 0, i - 4 ), b = std::min( n - 1, i + 4 );
		grade[i] = c[b].s > c[a].s ? (smoothLevel[a] - smoothLevel[b]) / (c[b].s - c[a].s) : 0.0f;
	}
	{
		std::vector<float> raw( n ), widened( n );
		for ( int i = 0; i < n; i++ )
		{
			const float change = fabsf( grade[ std::min( n - 1, i + 4 ) ] - grade[ std::max( 0, i - 4 ) ] );
			const float bend = std::min( 1.0f, std::max( 0.0f, (change - 0.03f) / 0.10f ) );
			const float steep = std::min( 1.0f, std::max( 0.0f, (grade[i] - 0.12f) / 0.25f ) );
			raw[i] = std::min( 1.0f, v.rapids * (bend + 0.4f * steep) );
		}
		for ( int i = 0; i < n; i++ )
		{
			float m = raw[i];
			for ( int j = std::max( 0, i - 2 ); j <= std::min( n - 1, i + 2 ); j++ ) m = std::max( m, raw[j] );
			widened[i] = m;
		}
		for ( int i = 0; i < n; i++ )
		{
			float sum = 0, fastSum = 0;
			int count = 0;
			for ( int j = std::max( 0, i - 5 ); j <= std::min( n - 1, i + 5 ); j++ )
			{
				sum += widened[j];
				fastSum += 1.0f + v.steepFlow * std::min( 2.0f, std::max( 0.0f, grade[j] ) / 0.15f );
				count++;
			}
			turbulence[i] = std::min( 1.0f, sum / count );
			speed[i] = std::min( 6.0f, fastSum / count );
		}
	}

	// where a side's terrain rises to the level, and a little further while it stays above it
	auto edge = [&]( int i, int side, float L )
	{
		const float* h = &sideH[ ((size_t)i * 2 + side) * steps ];
		int k = 0;
		while ( k < steps && h[k] < L ) k++;
		if ( k >= steps ) return (steps - 1) * step;
		float dist = 0;
		if ( k > 0 ) dist = ((k - 1) + (L - h[k-1]) / std::max( 0.001f, h[k] - h[k-1] )) * step;
		const float under = texel * 0.6f;
		float out = dist + under;
		for ( int j = k + 1; j < steps && j * step <= dist + under; j++ )
		{
			if ( h[j] < L ) { out = std::max( dist, (j - 1) * step ); break; }
		}
		return std::min( out, (steps - 1) * step );
	};

	// the sections with water, those below the sea, and the strip between wet sections not both below the sea
	std::vector<char> wet( n ), below( n );
	for ( int i = 0; i < n; i++ )
	{
		wet[i] = level[i] > c[i].ground + 15.0f;
		below[i] = level[i] <= sea + 2.0f;
	}
	// the rapids fade out over 40 m before the river meets the sea (no rapids run into it), and before its last node with
	// Calm River End
	{
		float nextSea = FLT_MAX;
		for ( int i = n - 1; i >= 0; i-- )
		{
			if ( below[i] ) nextSea = c[i].s;
			float calm = nextSea < FLT_MAX ? std::min( 1.0f, std::max( 0.0f, (nextSea - c[i].s) / 1575.0f ) ) : 1.0f;
			if ( v.calmEnd ) calm *= std::min( 1.0f, std::max( 0.0f, (c[ n - 1 ].s - c[i].s) / 1575.0f ) );
			turbulence[i] *= calm;
		}
	}
	const float tile = 600.0f;
	std::vector<float> positions, uvs, uvs2;
	std::vector<uint32_t> indices;
	std::vector<int> vertex( n, -1 );
	std::vector<float> drawLevel( n ), leftEdge( n ), rightEdge( n );
	auto addSection = [&]( int i )
	{
		if ( vertex[i] >= 0 ) return;
		drawLevel[i] = below[i] ? sea + 1.0f : level[i];
		leftEdge[i] = edge( i, 0, drawLevel[i] );
		rightEdge[i] = edge( i, 1, drawLevel[i] );
		vertex[i] = (int)(positions.size() / 3);
		const float left[3] = { c[i].x - nxs[i] * leftEdge[i], drawLevel[i], c[i].z - nzs[i] * leftEdge[i] };
		const float right[3] = { c[i].x + nxs[i] * rightEdge[i], drawLevel[i], c[i].z + nzs[i] * rightEdge[i] };
		positions.insert( positions.end(), left, left + 3 );
		positions.insert( positions.end(), right, right + 3 );
		const float uvLeft[2] = { -leftEdge[i] / tile, c[i].s / tile }, uvRight[2] = { rightEdge[i] / tile, c[i].s / tile };
		uvs.insert( uvs.end(), uvLeft, uvLeft + 2 );
		uvs.insert( uvs.end(), uvRight, uvRight + 2 );
		const float rough[4] = { turbulence[i], speed[i], turbulence[i], speed[i] };
		uvs2.insert( uvs2.end(), rough, rough + 4 );
	};
	std::vector<int> quads;
	for ( int i = 1; i < n; i++ )
	{
		if ( !wet[ i - 1 ] || !wet[i] || (below[ i - 1 ] && below[i]) ) continue;
		addSection( i - 1 );
		addSection( i );
		const uint32_t a0 = (uint32_t)vertex[ i - 1 ], b0 = (uint32_t)vertex[i];
		const uint32_t quad[6] = { a0, b0, a0 + 1, a0 + 1, b0, b0 + 1 };
		indices.insert( indices.end(), quad, quad + 6 );
		quads.push_back( i );
	}
	if ( indices.empty() ) return;
	const WickedCallWaterLook look = spline_waterlook( v );
	s.waterEntity = WickedCall_CreateWaterSurface( positions.data(), uvs.data(), (uint32_t)(positions.size() / 3), indices.data(), (uint32_t)indices.size(), look, uvs2.data() );
	s.waterLook = spline_waterlookhash( look );

	// the texels under the water, and its height, turbulence and current over each (the shader's flow: 0.03 texture tiles
	// a second at speed 1, a tile 600 units over its UV scale)
	const float baseFlow = 0.03f * look.speed * tile / std::max( 0.01f, look.uvScale );
	std::unordered_map<uint32_t, sRiverTexel> under;
	auto toTexel = [E]( float x ) { return (x / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE; };
	for ( int i : quads )
	{
		const int ia = i - 1, ib = i;
		const sRoadSample& a = c[ ia ];
		const sRoadSample& b = c[ ib ];
		const float wide = std::max( std::max( leftEdge[ia], rightEdge[ia] ), std::max( leftEdge[ib], rightEdge[ib] ) );
		const int ix0 = std::max( 0, (int)floorf( toTexel( std::min( a.x, b.x ) - wide ) ) ), ix1 = std::min( SPLINE_MAP_SIZE - 1, (int)ceilf( toTexel( std::max( a.x, b.x ) + wide ) ) );
		const int iz0 = std::max( 0, (int)floorf( toTexel( std::min( a.z, b.z ) - wide ) ) ), iz1 = std::min( SPLINE_MAP_SIZE - 1, (int)ceilf( toTexel( std::max( a.z, b.z ) + wide ) ) );
		const float dx = b.x - a.x, dz = b.z - a.z;
		const float len2 = dx * dx + dz * dz;
		const float nx = (nxs[ia] + nxs[ib]) * 0.5f, nz = (nzs[ia] + nzs[ib]) * 0.5f;
		for ( int iz = iz0; iz <= iz1; iz++ )
		{
			const float wz = ((float)iz / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
			for ( int ix = ix0; ix <= ix1; ix++ )
			{
				const float wx = ((float)ix / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
				float tt = len2 > 0 ? ((wx - a.x) * dx + (wz - a.z) * dz) / len2 : 0;
				if ( tt < -0.01f || tt > 1.01f ) continue;
				tt = std::min( 1.0f, std::max( 0.0f, tt ) );
				const float lateral = (wx - (a.x + dx * tt)) * nx + (wz - (a.z + dz * tt)) * nz;
				const float leftLimit = leftEdge[ia] + (leftEdge[ib] - leftEdge[ia]) * tt;
				const float rightLimit = rightEdge[ia] + (rightEdge[ib] - rightEdge[ia]) * tt;
				if ( lateral < -leftLimit || lateral > rightLimit ) continue;
				const uint32_t key = (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix;
				sRiverTexel texel;
				texel.height = drawLevel[ia] + (drawLevel[ib] - drawLevel[ia]) * tt;
				texel.turbulence = turbulence[ia] + (turbulence[ib] - turbulence[ia]) * tt;
				texel.wade = s.river.wadeDepth;
				const float flow = baseFlow * (speed[ia] + (speed[ib] - speed[ia]) * tt);
				texel.flowX = (nzs[ia] + (nzs[ib] - nzs[ia]) * tt) * flow; // downstream is the normal turned back a quarter
				texel.flowZ = -(nxs[ia] + (nxs[ib] - nxs[ia]) * tt) * flow;
				auto it = under.find( key );
				if ( it == under.end() || texel.height > it->second.height ) under[ key ] = texel;
			}
		}
	}
	s.waterTexels.assign( under.begin(), under.end() );
}

// what a river's water surface is built from: its bake and its water depth
static uint64_t spline_watershape( const sSpline& s )
{
	if ( s.kind != SPLINE_KIND_RIVER || s.baked.empty() ) return 0;
	uint64_t h = spline_hashmix( 0xcbf29ce484222325ULL, &s.bakedSignature, sizeof(s.bakedSignature) );
	h = spline_hashmix( h, &s.bakeCount, sizeof(s.bakeCount) );
	h = spline_hashmix( h, &s.river.waterDepth, sizeof(s.river.waterDepth) );
	h = spline_hashmix( h, &s.river.rapids, sizeof(s.river.rapids) );
	h = spline_hashmix( h, &s.river.steepFlow, sizeof(s.river.steepFlow) );
	h = spline_hashmix( h, &s.river.calmEnd, sizeof(s.river.calmEnd) );
	h = spline_hashmix( h, &s.river.wadeDepth, sizeof(s.river.wadeDepth) );
	const WickedCallWaterLook look = spline_waterlook( s.river );
	h = spline_hashmix( h, &look.speed, sizeof(look.speed) );
	h = spline_hashmix( h, &look.uvScale, sizeof(look.uvScale) );
	h = spline_hashmix( h, &t.terrain.waterliney_f, sizeof(t.terrain.waterliney_f) );
	return h ? h : 1;
}

// builds the rivers' water surfaces that changed (their bake or water depth), once the terrain is ready (or now when
// forced), and gives a changed look to the rest; called every frame in the editor and in game
void spline_updatewater( bool bForce )
{
	if ( g_iSplineWaterWait > 0 ) g_iSplineWaterWait--;
	const bool bCanBuild = bForce || (g_iSplineWaterWait == 0 && GGTerrain::GGTerrain_IsReady());
	bool bMapChanged = false;
	for ( sSpline& s : g_Splines )
	{
		const uint64_t shape = spline_watershape( s );
		if ( shape != s.waterShape )
		{
			if ( !bCanBuild ) continue;
			spline_buildwater( s );
			s.waterShape = shape;
			bMapChanged = true;
			continue;
		}
		if ( s.waterEntity )
		{
			const WickedCallWaterLook look = spline_waterlook( s.river );
			const uint64_t lookHash = spline_waterlookhash( look );
			if ( lookHash != s.waterLook )
			{
				WickedCall_SetWaterSurfaceLook( s.waterEntity, look );
				s.waterLook = lookHash;
			}
		}
	}
	if ( bMapChanged ) spline_rebuildwatermap();
}

// the water's height at a point: a river's where one flows over it, else the level's water line
float spline_waterheightat( float x, float z, int* pIsRiver )
{
	if ( pIsRiver ) *pIsRiver = 0;
	float h = t.terrain.waterliney_f;
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( g_RiverWater.empty() || E <= 0 ) return h;
	const int ix = (int)((x / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f), iz = (int)((z / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f);
	if ( ix < 0 || iz < 0 || ix >= SPLINE_MAP_SIZE || iz >= SPLINE_MAP_SIZE ) return h;
	auto it = g_RiverWater.find( (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix );
	if ( it != g_RiverWater.end() && it->second.height > h )
	{
		h = it->second.height;
		if ( pIsRiver ) *pIsRiver = 1;
	}
	return h;
}

// the current of a river's water at a point (units a second, downstream), 0 where no river flows above the sea
void spline_waterflowat( float x, float z, float* pFlowX, float* pFlowZ )
{
	*pFlowX = *pFlowZ = 0.0f;
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( g_RiverWater.empty() || E <= 0 ) return;
	const int ix = (int)((x / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f), iz = (int)((z / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f);
	if ( ix < 0 || iz < 0 || ix >= SPLINE_MAP_SIZE || iz >= SPLINE_MAP_SIZE ) return;
	auto it = g_RiverWater.find( (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix );
	if ( it == g_RiverWater.end() || it->second.height <= t.terrain.waterliney_f ) return;
	*pFlowX = it->second.flowX;
	*pFlowZ = it->second.flowZ;
}

// how turbulent a river's water is at a point, 0 (smooth, or no river above the sea there) to 1 (rapids)
float spline_riverturbulenceat( float x, float z )
{
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( g_RiverWater.empty() || E <= 0 ) return 0.0f;
	const int ix = (int)((x / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f), iz = (int)((z / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f);
	if ( ix < 0 || iz < 0 || ix >= SPLINE_MAP_SIZE || iz >= SPLINE_MAP_SIZE ) return 0.0f;
	auto it = g_RiverWater.find( (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix );
	if ( it == g_RiverWater.end() || it->second.height <= t.terrain.waterliney_f ) return 0.0f;
	return it->second.turbulence;
}

// for the navmesh bake: the level under the rivers' water above which the ground is walkable at a point, the water's height
// less the river's wade depth (none: -1e30), and a hash of those levels over a rect (0 none)
float spline_riverwaterlevel( float x, float z )
{
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( g_RiverWater.empty() || E <= 0 ) return -1e30f;
	const int ix = (int)((x / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f), iz = (int)((z / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f);
	if ( ix < 0 || iz < 0 || ix >= SPLINE_MAP_SIZE || iz >= SPLINE_MAP_SIZE ) return -1e30f;
	auto it = g_RiverWater.find( (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix );
	return it != g_RiverWater.end() ? it->second.height - it->second.wade : -1e30f;
}

uint64_t spline_riverwaterinputs( float minX, float minZ, float maxX, float maxZ )
{
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( g_RiverWater.empty() || E <= 0 ) return 0;
	const int ix0 = std::max( 0, (int)floorf( (minX / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE ) ), ix1 = std::min( SPLINE_MAP_SIZE - 1, (int)ceilf( (maxX / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE ) );
	const int iz0 = std::max( 0, (int)floorf( (minZ / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE ) ), iz1 = std::min( SPLINE_MAP_SIZE - 1, (int)ceilf( (maxZ / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE ) );
	uint64_t h = 0xcbf29ce484222325ULL;
	bool bAny = false;
	if ( (uint64_t)(ix1 - ix0 + 1) * (uint64_t)(iz1 - iz0 + 1) > g_RiverWater.size() * 4 )
	{
		// a large rect (the whole map): every river texel, in key order so the hash doesn't depend on the map's order
		std::vector<std::pair<uint32_t, float>> all;
		all.reserve( g_RiverWater.size() );
		for ( const auto& texel : g_RiverWater ) all.push_back( { texel.first, texel.second.height - texel.second.wade } );
		std::sort( all.begin(), all.end() );
		for ( const auto& texel : all )
		{
			const int ix = (int)(texel.first % SPLINE_MAP_SIZE), iz = (int)(texel.first / SPLINE_MAP_SIZE);
			if ( ix < ix0 || ix > ix1 || iz < iz0 || iz > iz1 ) continue;
			h = spline_hashmix( h, &texel.first, sizeof(texel.first) );
			h = spline_hashmix( h, &texel.second, sizeof(texel.second) );
			bAny = true;
		}
	}
	else
	{
		for ( int iz = iz0; iz <= iz1; iz++ )
		{
			for ( int ix = ix0; ix <= ix1; ix++ )
			{
				const uint32_t key = (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix;
				auto it = g_RiverWater.find( key );
				if ( it == g_RiverWater.end() ) continue;
				const float level = it->second.height - it->second.wade;
				h = spline_hashmix( h, &key, sizeof(key) );
				h = spline_hashmix( h, &level, sizeof(level) );
				bAny = true;
			}
		}
	}
	return bAny ? (h ? h : 1) : 0;
}

//
// Placement layers
//

static float spline_random( uint32_t a, uint32_t b, uint32_t c, uint32_t salt )
{
	uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du ^ salt * 0x27D4EB2Fu;
	h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
	return (h & 0xFFFFFF) / 16777215.0f;
}

// the area a spline's nodes cover, widened by a margin
static void spline_bounds( const sSpline& s, float margin, float* pBounds )
{
	pBounds[0] = pBounds[1] = FLT_MAX;
	pBounds[2] = pBounds[3] = -FLT_MAX;
	for ( const sSplineNode& node : s.nodes )
	{
		pBounds[0] = std::min( pBounds[0], node.x + std::min( 0.0f, std::min( node.inX, node.outX ) ) );
		pBounds[1] = std::min( pBounds[1], node.z + std::min( 0.0f, std::min( node.inZ, node.outZ ) ) );
		pBounds[2] = std::max( pBounds[2], node.x + std::max( 0.0f, std::max( node.inX, node.outX ) ) );
		pBounds[3] = std::max( pBounds[3], node.z + std::max( 0.0f, std::max( node.inZ, node.outZ ) ) );
	}
	pBounds[0] -= margin; pBounds[1] -= margin; pBounds[2] += margin; pBounds[3] += margin;
}

static bool spline_overlap( const float* a, const float* b )
{
	return a[0] <= b[2] && b[0] <= a[2] && a[1] <= b[3] && b[1] <= a[3];
}

// what the layers depend on: the layers, the spline's kind and its bake, and the splines near it (a road crossing or joining
// it moves where its entities may go; one earlier in the list also by what it placed, which this one keeps apart from)
static uint64_t spline_layersignature( int si )
{
	const sSpline& s = g_Splines[ si ];
	uint64_t h = 0xcbf29ce484222325ULL;
	h = spline_hashmix( h, &s.kind, sizeof(s.kind) );
	h = spline_hashmix( h, &s.bakedSignature, sizeof(s.bakedSignature) );
	for ( const sSplineLayer& layer : s.layers ) h = spline_hashmix( h, &layer, sizeof(layer) );
	if ( s.layers.empty() ) return h ? h : 1;
	float bounds[4], other[4];
	spline_bounds( s, 3000.0f, bounds );
	for ( int sj = 0; sj < (int)g_Splines.size(); sj++ )
	{
		if ( sj == si ) continue;
		const sSpline& o = g_Splines[ sj ];
		if ( o.kind == SPLINE_KIND_NONE ) continue;
		spline_bounds( o, 0.0f, other );
		if ( !spline_overlap( bounds, other ) ) continue;
		h = spline_hashmix( h, &o.kind, sizeof(o.kind) );
		h = spline_hashmix( h, &o.bakedSignature, sizeof(o.bakedSignature) );
		if ( sj < si ) h = spline_hashmix( h, &o.placedSignature, sizeof(o.placedSignature) );
	}
	return h ? h : 1;
}

// the element's entity file is loaded and placed at a place and turn, scale in percent; 0 if it can't be
static int spline_addelement( const char* pEntity, float x, float y, float z, float yaw, float scale, const float* pUp = nullptr, int layFlat = 0 )
{
	char pPath[ MAX_PATH ];
	sprintf_s( pPath, MAX_PATH, "entitybank\\%s", pEntity );
	GG_GetRealPath( pPath, 0 );
	if ( pEntity[0] == 0 || FileExist( pPath ) == 0 ) return 0;
	const int storeGridEntity = t.gridentity;
	const entityeleproftype storeGridEleprof = t.grideleprof;
	t.addentityfile_s = pEntity;
	entity_adduniqueentity( false );
	const int entid = t.entid;
	if ( entid <= 0 ) return 0;
	t.gridentity = entid;
	entity_fillgrideleproffromprofile();
	t.gridentitystaticmode = t.entityprofile[ entid ].defaultstatic;
	if ( t.gridentitystaticmode == 1 ) g.projectmodifiedstatic = 1;
	t.gridentityposx_f = x; t.gridentityposy_f = y; t.gridentityposz_f = z;
	t.gridentityrotatex_f = 0; t.gridentityrotatey_f = yaw; t.gridentityrotatez_f = 0;
	t.gridentityrotatequatmode = 0;
	t.gridentityrotatequatx_f = 0; t.gridentityrotatequaty_f = 0; t.gridentityrotatequatz_f = 0; t.gridentityrotatequatw_f = 1;
	t.gridentityscalex_f = scale; t.gridentityscaley_f = scale; t.gridentityscalez_f = scale;
	t.gridentityoverwritemode = 0;
	t.gridentitypreferelementindex = 0;
	t.gridentityhasparent = 0;
	t.gridentityeditorfixed = 0;
	extern bool bNextObjectMustBeClone;
	extern bool bUpdateObjectList;
	bNextObjectMustBeClone = true;
	t.e = 0;
	entity_addentitytomap();
	bNextObjectMustBeClone = false;
	bUpdateObjectList = true;
	const int e = t.e;
	t.gridentity = storeGridEntity;
	t.grideleprof = storeGridEleprof;
	if ( e > 0 && (pUp || layFlat) && t.entityelement[ e ].obj > 0 && ObjectExist( t.entityelement[ e ].obj ) )
	{
		// laid flat about its own X first (a model standing up, a wall decal, turned onto its back), turned to its heading,
		// then tilted the shortest way from upright to the ground's normal (as the editor's own rotation, the object turned
		// by the quaternion and the angles read back)
		GGVECTOR3 up( 0, 1, 0 );
		GGQUATERNION quat;
		GGQuaternionRotationAxis( &quat, &up, GGToRadian( yaw ) );
		if ( layFlat )
		{
			// +90 turns the model's face (-Z, where a model faces the camera) up
			GGVECTOR3 side( 1, 0, 0 );
			GGQUATERNION quatFlat;
			GGQuaternionRotationAxis( &quatFlat, &side, GGToRadian( layFlat == 1 ? 90.0f : -90.0f ) );
			quat = quatFlat * quat;
		}
		if ( pUp )
		{
			GGVECTOR3 normal( pUp[0], pUp[1], pUp[2] );
			GGVec3Normalize( &normal, &normal );
			GGVECTOR3 axis;
			GGVec3Cross( &axis, &up, &normal );
			const float axisLength = sqrtf( axis.x * axis.x + axis.y * axis.y + axis.z * axis.z );
			if ( axisLength > 0.0001f )
			{
				axis /= axisLength;
				GGQUATERNION quatTilt;
				GGQuaternionRotationAxis( &quatTilt, &axis, acosf( std::min( 1.0f, std::max( -1.0f, normal.y ) ) ) );
				quat = quat * quatTilt;
			}
		}
		{
			const int obj = t.entityelement[ e ].obj;
			RotateObjectQuat( obj, quat.x, quat.y, quat.z, quat.w );
			t.entityelement[ e ].rx = ObjectAngleX( obj );
			t.entityelement[ e ].ry = ObjectAngleY( obj );
			t.entityelement[ e ].rz = ObjectAngleZ( obj );
			t.entityelement[ e ].quatmode = 1;
			t.entityelement[ e ].quatx = quat.x;
			t.entityelement[ e ].quaty = quat.y;
			t.entityelement[ e ].quatz = quat.z;
			t.entityelement[ e ].quatw = quat.w;
			if ( t.entityelement[ e ].staticflag == 1 ) g.projectmodifiedstatic = 1;
		}
	}
	return e;
}

// the highest N of the level's entities named <base>__N (any case), 0 if none
static int spline_lastnamenumber( const std::string& base, int except )
{
	int last = 0;
	const size_t len = base.size();
	for ( int e = 1; e <= g.entityelementlist; e++ )
	{
		if ( e == except || t.entityelement[ e ].maintype == 0 || t.entityelement[ e ].bankindex <= 0 ) continue;
		const char* pName = t.entityelement[ e ].eleprof.name_s.Get();
		if ( !pName || _strnicmp( pName, base.c_str(), len ) != 0 || pName[ len ] != '_' || pName[ len + 1 ] != '_' || !pName[ len + 2 ] ) continue;
		bool bDigits = true;
		for ( const char* pDigit = pName + len + 2; *pDigit; pDigit++ ) if ( !isdigit( (unsigned char)*pDigit ) ) { bDigits = false; break; }
		if ( bDigits ) last = std::max( last, atoi( pName + len + 2 ) );
	}
	return last;
}

// the name an entity is given when placed: its .fpe's desc
static const char* spline_entitydesc( const char* pEntity )
{
	static std::unordered_map<std::string, std::string> descs;
	auto it = descs.find( pEntity );
	if ( it != descs.end() ) return it->second.c_str();
	std::string desc;
	char pPath[ MAX_PATH ];
	sprintf_s( pPath, MAX_PATH, "entitybank\\%s", pEntity );
	GG_GetRealPath( pPath, 0 );
	FILE* fp = nullptr;
	if ( pEntity[0] && fopen_s( &fp, pPath, "r" ) == 0 && fp )
	{
		char line[ 512 ];
		while ( fgets( line, 512, fp ) )
		{
			char* p = line;
			while ( *p == ' ' || *p == '\t' ) p++;
			if ( _strnicmp( p, "desc", 4 ) != 0 ) continue;
			p += 4;
			while ( *p == ' ' || *p == '\t' ) p++;
			if ( *p != '=' ) continue;
			p++;
			while ( *p == ' ' || *p == '\t' ) p++;
			char* pEnd = p + strlen( p );
			while ( pEnd > p && (pEnd[-1] == '\n' || pEnd[-1] == '\r' || pEnd[-1] == ' ' || pEnd[-1] == '\t') ) *--pEnd = 0;
			desc = p;
			break;
		}
		fclose( fp );
	}
	return (descs[ pEntity ] = desc).c_str();
}

// takes away the entities the spline's layers placed, those still where they were put (one moved by hand stays)
static void spline_unplace( sSpline& s )
{
	if ( s.placed.empty() ) return;
	extern bool DeleteEntityFromLists( int e );
	// entities carrying lights rebuild the light list once, after all are gone
	lighting_holdrefresh( true );
	for ( int e = 1; e <= g.entityelementlist; e++ )
	{
		if ( t.entityelement[ e ].maintype == 0 || t.entityelement[ e ].bankindex <= 0 ) continue;
		if ( t.entityelement[ e ].eleprof.iObjectReserved1 != SPLINE_ENTITY_TAG || t.entityelement[ e ].eleprof.iObjectReserved2 != s.id ) continue;
		bool bStill = false;
		for ( const sSplinePlaced& placed : s.placed )
		{
			if ( fabsf( placed.x - t.entityelement[ e ].x ) < 2.0f && fabsf( placed.z - t.entityelement[ e ].z ) < 2.0f && fabsf( placed.y - t.entityelement[ e ].y ) < 2.0f )
			{
				const bool bFrozen = placed.layer >= 0 && placed.layer < (int)s.layers.size() && s.layers[ placed.layer ].frozen;
				bStill = !bFrozen;
				break;
			}
		}
		if ( !bStill ) continue;
		t.tentitytoselect = e;
		DeleteEntityFromLists( e );
		entity_deleteentityfrommap();
		t.tentitytoselect = 0;
	}
	lighting_holdrefresh( false );
	// a frozen layer's entities stay, and stay known
	std::vector<sSplinePlaced> kept;
	for ( const sSplinePlaced& placed : s.placed )
	{
		if ( placed.layer >= 0 && placed.layer < (int)s.layers.size() && s.layers[ placed.layer ].frozen ) kept.push_back( placed );
	}
	s.placed.swap( kept );
	g.projectmodified = 1;
}

// a road's carriageway as a line and its half width, for keeping other splines' entities off it
struct sSplineAvoid
{
	std::vector<sSplinePoint> line;
	float halfWidth = 0;
	float bounds[4];
};

// whether a point is on one of these carriageways: within half its width of its line, which ends square at its first and
// last points (another road joined end to end runs on up to the junction, rather than stopping half a width short of a
// round end) and is round at its bends (so a point outside a bend isn't missed between two pieces)
static bool spline_oncarriageway( const std::vector<sSplineAvoid>& roads, float x, float z )
{
	for ( const sSplineAvoid& road : roads )
	{
		if ( x < road.bounds[0] || x > road.bounds[2] || z < road.bounds[1] || z > road.bounds[3] ) continue;
		const size_t last = road.line.size() - 1;
		for ( size_t k = 1; k < road.line.size(); k++ )
		{
			const sSplinePoint& a = road.line[ k - 1 ];
			const sSplinePoint& b = road.line[ k ];
			const float dx = b.x - a.x, dz = b.z - a.z;
			const float len2 = dx * dx + dz * dz;
			float tt = len2 > 0 ? ((x - a.x) * dx + (z - a.z) * dz) / len2 : 0;
			if ( (k == 1 && tt < 0) || (k == last && tt > 1) ) continue;
			tt = std::min( 1.0f, std::max( 0.0f, tt ) );
			const float px = a.x + dx * tt - x, pz = a.z + dz * tt - z;
			if ( px * px + pz * pz < road.halfWidth * road.halfWidth ) return true;
		}
	}
	return false;
}

// a road's shape as its markings follow it (its nodes, curve and width)
static uint64_t spline_markingshape( const sSpline& s )
{
	uint64_t h = spline_hashmix( 0xcbf29ce484222325ULL, &s.curve, sizeof(s.curve) );
	h = spline_hashmix( h, &s.closed, sizeof(s.closed) );
	h = spline_hashmix( h, &s.road.width, sizeof(s.road.width) );
	for ( const sSplineNode& node : s.nodes )
	{
		const float f[6] = { node.x, node.z, node.inX, node.inZ, node.outX, node.outZ };
		h = spline_hashmix( h, f, sizeof(f) );
		h = spline_hashmix( h, &node.segCurve, sizeof(node.segCurve) );
	}
	return h;
}

// a road's painted lines (its Markings) as pieces about 3 m long for the terrain's pages (GGTerrain_SetMarkings), and
// their bounds. None on an earlier road's carriageway in the list, so a road that joins or crosses an earlier one stops
// its lines at the earlier one's edge, whose lines run on as a main road's do. The centre line goes solid where the road
// bends tighter than its Solid on Bends, with dashes three times longer than their gaps before and after (warning lines);
// a segment's own markings take over the road's
static void spline_markings( int si, std::vector<GGTerrain::GGTerrainMarking>& out, float* pBounds )
{
	pBounds[0] = pBounds[1] = 1e30f;
	pBounds[2] = pBounds[3] = -1e30f;
	const sSpline& s = g_Splines[ si ];
	const sSplineRoad& r = s.road;
	const int lanes = std::max( 1, std::min( 4, r.markLanes ) );
	if ( s.kind != SPLINE_KIND_ROAD || (r.markCentre == SPLINE_MARK_NONE && lanes < 2 && !r.markEdges) ) return;
	std::vector<sRoadSample> c;
	std::vector<int> nodeSample;
	if ( !spline_centreline( s, 118.0f, c, nodeSample ) ) return;
	const int n = (int)c.size();

	// each sample's segment and its left normal
	const int segs = spline_segments( s );
	std::vector<int> seg( n, segs - 1 );
	for ( int k = 0; k < segs; k++ )
	{
		const int first = nodeSample[ k ];
		const int last = k + 1 < segs ? nodeSample[ k + 1 ] : n - 1;
		for ( int i = std::max( 0, first ); i < last && i < n; i++ ) seg[ i ] = k;
	}
	std::vector<float> nx( n ), nz( n );
	for ( int i = 0; i < n; i++ )
	{
		const sRoadSample& a = c[ std::max( 0, i - 1 ) ];
		const sRoadSample& b = c[ std::min( n - 1, i + 1 ) ];
		float tx = b.x - a.x, tz = b.z - a.z;
		const float len = sqrtf( tx * tx + tz * tz );
		if ( len > 0 ) { tx /= len; tz /= len; }
		nx[ i ] = -tz;
		nz[ i ] = tx;
	}

	// the centre line's style at each sample: a segment's own markings, then a bend tighter than Solid on Bends (and 15 m
	// either side) solid; the dashed stretches within three dash lengths of a solid one forced so are warning lines
	std::vector<int> centre( n, r.markCentre );
	std::vector<char> forced( n, 0 );
	if ( r.markBendRadius > 0 )
	{
		std::vector<char> bend( n, 0 );
		for ( int i = 1; i < n - 1; i++ )
		{
			const float a = atan2f( c[i].z - c[i-1].z, c[i].x - c[i-1].x );
			const float b = atan2f( c[i+1].z - c[i].z, c[i+1].x - c[i].x );
			float turn = fabsf( b - a );
			if ( turn > 3.14159265f ) turn = 6.2831853f - turn;
			const float ds = (c[i+1].s - c[i-1].s) * 0.5f;
			if ( turn > 0.00001f && ds / turn < r.markBendRadius ) bend[ i ] = 1;
		}
		for ( int i = 0, lo = 0, hi = 0; i < n; i++ )
		{
			while ( c[ lo ].s < c[ i ].s - 590.0f ) lo++;
			while ( hi < n - 1 && c[ hi + 1 ].s <= c[ i ].s + 590.0f ) hi++;
			for ( int k = lo; k <= hi && !forced[ i ]; k++ ) if ( bend[ k ] ) forced[ i ] = 1;
		}
	}
	for ( int i = 0; i < n; i++ )
	{
		const int segMark = s.nodes[ seg[ i ] ].segMark;
		int style = r.markCentre;
		if ( segMark == SPLINE_SEGMARK_NONE ) style = SPLINE_MARK_NONE;
		else if ( segMark == SPLINE_SEGMARK_DASHED )
		{
			if ( style != SPLINE_MARK_NONE ) style = SPLINE_MARK_DASHED;
			forced[ i ] = 0;
		}
		else if ( segMark == SPLINE_SEGMARK_SOLID ) forced[ i ] = 1;
		if ( forced[ i ] )
		{
			if ( style == SPLINE_MARK_DASHED ) style = SPLINE_MARK_SOLID;
			else if ( style == SPLINE_MARK_SOLIDDASHED ) style = SPLINE_MARK_DOUBLE;
			else forced[ i ] = 0;
		}
		centre[ i ] = style;
	}
	const float period = r.markDash + r.markGap;
	std::vector<char> warning( n, 0 );
	for ( int i = 0; i < n; i++ )
	{
		if ( centre[ i ] != SPLINE_MARK_DASHED && centre[ i ] != SPLINE_MARK_SOLIDDASHED ) continue;
		for ( int k = i; k >= 0 && c[ i ].s - c[ k ].s < period * 3 && !warning[ i ]; k-- ) if ( forced[ k ] ) warning[ i ] = 1;
		for ( int k = i; k < n && c[ k ].s - c[ i ].s < period * 3 && !warning[ i ]; k++ ) if ( forced[ k ] ) warning[ i ] = 1;
	}

	// the earlier roads' carriageways
	std::vector<sSplineAvoid> avoid;
	{
		float bounds[4];
		spline_bounds( s, s.road.width, bounds );
		for ( int sj = 0; sj < si; sj++ )
		{
			const sSpline& o = g_Splines[ sj ];
			if ( o.kind != SPLINE_KIND_ROAD ) continue;
			sSplineAvoid road;
			spline_bounds( o, o.road.width * 0.5f + 100.0f, road.bounds );
			if ( !spline_overlap( bounds, road.bounds ) ) continue;
			spline_sample( o, 100.0f, road.line );
			road.halfWidth = o.road.width * 0.5f;
			avoid.push_back( road );
		}
	}
	auto onEarlierRoad = [&avoid]( float x, float z ) { return spline_oncarriageway( avoid, x, z ); };

	// the lines: two for the centre (one, or a pair either side of it), the lanes' each side, the edges; each keeps its own
	// length along it, so its dashes run on from piece to piece
	const float white[3] = { 0.72f, 0.72f, 0.68f }, yellow[3] = { 0.75f, 0.48f, 0.04f };
	const float* centreColour = r.markCentreYellow ? yellow : white;
	const float* edgeColour = r.markEdgeYellow ? yellow : white;
	const float half = r.width * 0.5f;
	const int slots = 2 + (lanes - 1) * 2 + 2;
	std::vector<float> phase( slots, 0.0f );
	for ( int i = 0; i < n - 1; i++ )
	{
		const int segMark = s.nodes[ seg[ i ] ].segMark;
		for ( int slot = 0; slot < slots; slot++ )
		{
			float offset = 0, width = r.markLineWidth, dash = 0, gap = 0;
			const float* colour = white;
			bool bLine = segMark != SPLINE_SEGMARK_NONE;
			if ( slot < 2 )
			{
				colour = centreColour;
				const int style = centre[ i ];
				const float pair = r.markLineWidth;
				if ( style == SPLINE_MARK_NONE ) bLine = false;
				else if ( style == SPLINE_MARK_DOUBLE ) offset = slot == 0 ? pair : -pair;
				else if ( style == SPLINE_MARK_SOLIDDASHED )
				{
					offset = slot == 0 ? pair : -pair;
					if ( slot == 1 ) { dash = r.markDash; gap = r.markGap; }
				}
				else
				{
					if ( slot == 1 ) bLine = false;
					if ( style == SPLINE_MARK_DASHED ) { dash = r.markDash; gap = r.markGap; }
				}
				if ( gap > 0 && warning[ i ] ) { dash = period * 0.75f; gap = period * 0.25f; }
			}
			else if ( slot < slots - 2 )
			{
				const int lane = (slot - 2) / 2 + 1;
				offset = (slot & 1 ? -1.0f : 1.0f) * lane * r.width / (lanes * 2);
				dash = r.markDash;
				gap = r.markGap;
			}
			else
			{
				colour = edgeColour;
				width = r.markEdgeWidth;
				offset = (slot == slots - 2 ? 1.0f : -1.0f) * (half - r.markEdgeInset - r.markEdgeWidth * 0.5f);
				bLine = bLine && r.markEdges;
			}
			const float ax = c[i].x + nx[i] * offset, az = c[i].z + nz[i] * offset;
			const float bx = c[i+1].x + nx[i+1] * offset, bz = c[i+1].z + nz[i+1] * offset;
			const float length = sqrtf( (bx - ax) * (bx - ax) + (bz - az) * (bz - az) );
			if ( bLine && !onEarlierRoad( (ax + bx) * 0.5f, (az + bz) * 0.5f ) )
			{
				GGTerrain::GGTerrainMarking piece;
				piece.ax = ax; piece.az = az; piece.bx = bx; piece.bz = bz;
				piece.halfWidth = width * 0.5f;
				piece.dash = dash;
				piece.gap = gap;
				piece.phase = phase[ slot ];
				piece.r = colour[0]; piece.g = colour[1]; piece.b = colour[2];
				piece.wear = r.markWear;
				out.push_back( piece );
				pBounds[0] = std::min( pBounds[0], std::min( ax, bx ) - width );
				pBounds[1] = std::min( pBounds[1], std::min( az, bz ) - width );
				pBounds[2] = std::max( pBounds[2], std::max( ax, bx ) + width );
				pBounds[3] = std::max( pBounds[3], std::max( az, bz ) + width );
			}
			phase[ slot ] += length;
		}
	}
}

// each road's markings as last given to the terrain (by spline id), and what they were made for
struct sSplineMarkings
{
	uint64_t signature = 0;
	std::vector<GGTerrain::GGTerrainMarking> pieces;
	float bounds[4] = { 1e30f, 1e30f, -1e30f, -1e30f };
};
static std::unordered_map<int, sSplineMarkings> g_SplineMarkings;
static uint64_t g_SplineMarkingsApplied = 0, g_SplineMarkingsPending = 0;
static int g_iSplineMarkingsStill = 0;

// gives the roads' markings to the terrain when they change (a road's shape, width or markings, or an earlier road's,
// which cuts its lines), the pages under what changed made again. A quarter second after the last change, so dragging a
// node doesn't remake the pages every frame; called every frame in the editor and in game
void spline_updatemarkings( void )
{
	if ( !GGTerrain::GGTerrain_IsReady() ) return;
	std::vector<uint64_t> signatures( g_Splines.size(), 0 );
	uint64_t earlier = 0xcbf29ce484222325ULL, total = earlier;
	for ( size_t si = 0; si < g_Splines.size(); si++ )
	{
		const sSpline& s = g_Splines[ si ];
		if ( s.kind != SPLINE_KIND_ROAD ) continue;
		const uint64_t shape = spline_markingshape( s );
		uint64_t h = spline_hashmix( earlier, &shape, sizeof(shape) );
		h = spline_hashmix( h, &s.road.markCentre, sizeof(sSplineRoad) - offsetof( sSplineRoad, markCentre ) );
		for ( const sSplineNode& node : s.nodes ) h = spline_hashmix( h, &node.segMark, sizeof(node.segMark) );
		signatures[ si ] = h ? h : 1;
		earlier = spline_hashmix( earlier, &shape, sizeof(shape) );
		total = spline_hashmix( total, &s.id, sizeof(s.id) );
		total = spline_hashmix( total, &signatures[ si ], sizeof(signatures[ si ]) );
	}
	if ( total == g_SplineMarkingsApplied ) return;
	if ( total != g_SplineMarkingsPending )
	{
		g_SplineMarkingsPending = total;
		g_iSplineMarkingsStill = 0;
		if ( g_SplineMarkingsApplied != 0 ) return;
	}
	if ( g_SplineMarkingsApplied != 0 && ++g_iSplineMarkingsStill < 15 ) return;

	// the roads that changed made again; the pages under what they had and have now
	std::vector<float> dirty;
	auto addDirty = [&dirty]( const float* b ) { if ( b[0] <= b[2] ) dirty.insert( dirty.end(), b, b + 4 ); };
	std::unordered_set<int> live;
	for ( size_t si = 0; si < g_Splines.size(); si++ )
	{
		const sSpline& s = g_Splines[ si ];
		if ( s.kind != SPLINE_KIND_ROAD ) continue;
		live.insert( s.id );
		sSplineMarkings& marks = g_SplineMarkings[ s.id ];
		if ( marks.signature == signatures[ si ] ) continue;
		addDirty( marks.bounds );
		marks.pieces.clear();
		spline_markings( (int)si, marks.pieces, marks.bounds );
		marks.signature = signatures[ si ];
		addDirty( marks.bounds );
	}
	for ( auto it = g_SplineMarkings.begin(); it != g_SplineMarkings.end(); )
	{
		if ( live.count( it->first ) ) { ++it; continue; }
		addDirty( it->second.bounds );
		it = g_SplineMarkings.erase( it );
	}
	std::vector<GGTerrain::GGTerrainMarking> all;
	for ( const sSpline& s : g_Splines )
	{
		auto it = g_SplineMarkings.find( s.id );
		if ( s.kind == SPLINE_KIND_ROAD && it != g_SplineMarkings.end() ) all.insert( all.end(), it->second.pieces.begin(), it->second.pieces.end() );
	}
	GGTerrain::GGTerrain_SetMarkings( all.data(), (uint32_t)all.size() );
	for ( size_t k = 0; k + 3 < dirty.size(); k += 4 ) GGTerrain::GGTerrain_InvalidateRegion( dirty[k], dirty[k+1], dirty[k+2], dirty[k+3], GGTERRAIN_INVALIDATE_TEXTURES );
	g_SplineMarkingsApplied = total;
}

// places the spline's layers along it, taking away what they placed before (a frozen layer's stay). None goes on another
// road's carriageway (a junction, a crossing), a road's in water (a river crossing it), nor within Keep Apart of one an
// earlier spline in the list placed of the same entity (no clumps where roads meet)
static void spline_place( int si )
{
	sSpline& s = g_Splines[ si ];
	spline_unplace( s );
	if ( s.kind != SPLINE_KIND_ROAD && s.kind != SPLINE_KIND_RIVER ) return;
	std::vector<sSplineAvoid> avoid;
	{
		float bounds[4];
		spline_bounds( s, 3000.0f, bounds );
		for ( int sj = 0; sj < (int)g_Splines.size(); sj++ )
		{
			const sSpline& o = g_Splines[ sj ];
			if ( sj == si || o.kind != SPLINE_KIND_ROAD || o.baked.empty() ) continue;
			sSplineAvoid road;
			spline_bounds( o, o.road.width * 0.5f + 100.0f, road.bounds );
			if ( !spline_overlap( bounds, road.bounds ) ) continue;
			spline_sample( o, 100.0f, road.line );
			road.halfWidth = o.road.width * 0.5f + 60.0f;
			avoid.push_back( road );
		}
	}
	auto onAnotherRoad = [&avoid]( float x, float z ) { return spline_oncarriageway( avoid, x, z ); };
	std::vector<sRoadSample> c;
	std::vector<int> nodeSample;
	if ( !spline_centreline( s, 50.0f, c, nodeSample ) ) return;
	const int n = (int)c.size();
	const float length = c[ n - 1 ].s;
	const float edge = s.kind == SPLINE_KIND_ROAD ? s.road.width * 0.5f : s.river.bedWidth * 0.5f;
	for ( int li = 0; li < (int)s.layers.size(); li++ )
	{
		const sSplineLayer& layer = s.layers[ li ];
		if ( !layer.enabled || layer.frozen || layer.entity[0] == 0 || layer.spacing < 10.0f ) continue;
		int k = 0, count = 0, number = 0;
		std::string baseName;
		for ( float along = layer.start; along <= length && count < 4000; along += layer.spacing, count++ )
		{
			int sides[2] = { 0, 0 }, sideCount = 0;
			switch ( layer.side )
			{
				case SPLINE_SIDE_LEFT: sides[ sideCount++ ] = -1; break;
				case SPLINE_SIDE_RIGHT: sides[ sideCount++ ] = 1; break;
				case SPLINE_SIDE_ALTERNATE: sides[ sideCount++ ] = (count & 1) ? 1 : -1; break;
				case SPLINE_SIDE_CENTRE: sides[ sideCount++ ] = 0; break;
				default: sides[ sideCount++ ] = -1; sides[ sideCount++ ] = 1; break;
			}
			for ( int iSide = 0; iSide < sideCount; iSide++ )
			{
				const int side = sides[ iSide ];
				const uint32_t salt = (uint32_t)(side + 1);
				float d = along + (spline_random( s.id, li, count, salt * 3 + 0 ) * 2.0f - 1.0f) * layer.jitter;
				d = std::min( length, std::max( 0.0f, d ) );
				while ( k > 0 && c[ k ].s > d ) k--;
				while ( k < n - 2 && c[ k + 1 ].s < d ) k++;
				const float span = c[ k + 1 ].s - c[ k ].s;
				const float tt = span > 0 ? (d - c[ k ].s) / span : 0.0f;
				float tx = c[ k + 1 ].x - c[ k ].x, tz = c[ k + 1 ].z - c[ k ].z;
				const float len = sqrtf( tx * tx + tz * tz );
				if ( len > 0 ) { tx /= len; tz /= len; }
				const float nx = -tz, nz = tx; // right of the way downstream
				float lateral = side == 0 ? layer.offset : side * (edge + layer.offset);
				lateral += (spline_random( s.id, li, count, salt * 3 + 1 ) * 2.0f - 1.0f) * layer.jitterAcross;
				const float px = c[ k ].x + (c[ k + 1 ].x - c[ k ].x) * tt + nx * lateral;
				const float pz = c[ k ].z + (c[ k + 1 ].z - c[ k ].z) * tt + nz * lateral;
				if ( s.kind == SPLINE_KIND_RIVER && layer.minTurbulence > 0.0f && spline_riverturbulenceat( px, pz ) < layer.minTurbulence ) continue;
				if ( onAnotherRoad( px, pz ) ) continue;
				float py = 0;
				if ( !GGTerrain::GGTerrain_GetHeight( px, pz, &py, 1, 1 ) || py != py ) py = spline_groundy( px, pz );
				if ( s.kind == SPLINE_KIND_ROAD )
				{
					int isRiver = 0;
					if ( spline_waterheightat( px, pz, &isRiver ) > py + 5.0f ) continue; // in water
				}
				if ( layer.keepApart > 0.0f )
				{
					bool bTooNear = false;
					const float apart2 = layer.keepApart * layer.keepApart;
					for ( int sj = 0; sj < si && !bTooNear; sj++ )
					{
						const sSpline& o = g_Splines[ sj ];
						for ( const sSplinePlaced& other : o.placed )
						{
							if ( other.layer < 0 || other.layer >= (int)o.layers.size() || _stricmp( o.layers[ other.layer ].entity, layer.entity ) != 0 ) continue;
							if ( (other.x - px) * (other.x - px) + (other.z - pz) * (other.z - pz) < apart2 ) { bTooNear = true; break; }
						}
					}
					if ( bTooNear ) continue;
				}
				py += layer.height;
				const float heading = GGToDegree( atan2f( tx, tz ) );
				float yaw = heading;
				if ( layer.facing == SPLINE_FACE_MIRRORED && side < 0 ) yaw = heading + 180.0f;
				if ( layer.facing == SPLINE_FACE_RANDOM ) yaw = spline_random( s.id, li, count, salt * 3 + 2 ) * 360.0f;
				yaw += layer.turn;
				while ( yaw >= 360.0f ) yaw -= 360.0f;
				while ( yaw < 0.0f ) yaw += 360.0f;
				const float scale = layer.scaleMin + (layer.scaleMax - layer.scaleMin) * spline_random( s.id, li, count, salt * 3 + 3 );
				float normal[3] = { 0, 1, 0 };
				if ( layer.followSlope )
				{
					// the ground's slope across 60 u
					const float step = 30.0f;
					float hx[2], hz[2];
					for ( int j = 0; j < 2; j++ )
					{
						const float dd = j ? step : -step;
						if ( !GGTerrain::GGTerrain_GetHeight( px + dd, pz, &hx[j], 1, 1 ) || hx[j] != hx[j] ) hx[j] = spline_groundy( px + dd, pz );
						if ( !GGTerrain::GGTerrain_GetHeight( px, pz + dd, &hz[j], 1, 1 ) || hz[j] != hz[j] ) hz[j] = spline_groundy( px, pz + dd );
					}
					normal[0] = -(hx[1] - hx[0]) / (2.0f * step);
					normal[2] = -(hz[1] - hz[0]) / (2.0f * step);
				}
				const int e = spline_addelement( layer.entity, px, py, pz, yaw, scale, layer.followSlope ? normal : nullptr, layer.layFlat );
				if ( e <= 0 ) break;
				t.entityelement[ e ].eleprof.iObjectReserved1 = SPLINE_ENTITY_TAG;
				t.entityelement[ e ].eleprof.iObjectReserved2 = s.id;
				t.entityelement[ e ].eleprof.iObjectReserved3 = li;
				// named as the entity (the game's systems find entities by name) or as the layer says, with __NN after it, on
				// from the highest of that name already in the level
				if ( baseName.empty() )
				{
					baseName = layer.name[0] ? layer.name : t.entityelement[ e ].eleprof.name_s.Get();
					number = spline_lastnamenumber( baseName, e );
				}
				if ( !baseName.empty() )
				{
					char suffix[ 16 ];
					sprintf_s( suffix, 16, "__%02d", ++number );
					t.entityelement[ e ].eleprof.name_s = (baseName + suffix).c_str();
				}
				sSplinePlaced placed;
				placed.layer = li;
				placed.x = t.entityelement[ e ].x;
				placed.y = t.entityelement[ e ].y;
				placed.z = t.entityelement[ e ].z;
				s.placed.push_back( placed );
			}
		}
	}
	g.projectmodified = 1;
}

// places the layers of the splines whose layers or bake changed, once nothing is being dragged or edited, a river's once
// its water is built (its rapids)
static void spline_placechanged( void )
{
	if ( iDragNode >= 0 || ImGui::IsAnyItemActive() ) return;
	if ( ImGui::GetFrameCount() - g_iSplineLoadFrame < SPLINE_LOAD_SETTLE_FRAMES ) return;
	// in list order, so a spline placing again is seen by the later ones near it in the same pass; entities carrying lights
	// (lamps) rebuild the light list once, after the pass
	bool bHeld = false;
	for ( int si = 0; si < (int)g_Splines.size(); si++ )
	{
		sSpline& s = g_Splines[ si ];
		const uint64_t signature = spline_layersignature( si );
		if ( signature == s.placedSignature ) continue;
		if ( s.kind == SPLINE_KIND_RIVER && !s.baked.empty() && s.waterShape != spline_watershape( s ) ) continue;
		if ( !bHeld ) { lighting_holdrefresh( true ); bHeld = true; }
		spline_place( si );
		s.placedSignature = spline_layersignature( si );
	}
	if ( bHeld ) lighting_holdrefresh( false );
}

// a new layer, set for the spline's kind: the Long Bien bridge's streetlights along a road, rocks along a river
static sSplineLayer spline_newlayer( const sSpline& s )
{
	sSplineLayer layer;
	if ( s.kind == SPLINE_KIND_ROAD )
	{
		strcpy_s( layer.entity, 260, "User\\Props\\Streetlights\\rusty_streetlight.fpe" );
		layer.spacing = 1213.0f;
		layer.offset = 20.0f;
		layer.side = SPLINE_SIDE_BOTH;
		layer.facing = SPLINE_FACE_MIRRORED;
		layer.keepApart = 600.0f;
	}
	else
	{
		layer.spacing = 600.0f;
		layer.offset = 150.0f;
		layer.side = SPLINE_SIDE_BOTH;
		layer.facing = SPLINE_FACE_RANDOM;
		layer.scaleMin = 70.0f;
		layer.scaleMax = 140.0f;
		layer.jitter = 150.0f;
		layer.jitterAcross = 150.0f;
		layer.keepApart = 0.0f;
	}
	return layer;
}

//
// The 3D view
//

static bool spline_project( float x, float y, float z, ImVec2* pOut )
{
	ImVec2 other;
	if ( !Convert3DLineTo2D( x, y, z, x, y, z, pOut, &other ) ) return false;
	*pOut += ImGui::GetMainViewport()->Pos;
	return true;
}

static bool spline_projectline( float x1, float y1, float z1, float x2, float y2, float z2, ImVec2* pA, ImVec2* pB )
{
	if ( !Convert3DLineTo2D( x1, y1, z1, x2, y2, z2, pA, pB ) ) return false;
	*pA += ImGui::GetMainViewport()->Pos;
	*pB += ImGui::GetMainViewport()->Pos;
	return true;
}

static float spline_distance2d( ImVec2 a, ImVec2 b )
{
	return sqrtf( (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) );
}

// the distance from p to the screen segment a-b, and how far along it the nearest point is
static float spline_distancetosegment( ImVec2 p, ImVec2 a, ImVec2 b, float* pAlong )
{
	const float dx = b.x - a.x, dy = b.y - a.y;
	const float len2 = dx * dx + dy * dy;
	float t = len2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0;
	if ( t < 0 ) t = 0;
	if ( t > 1 ) t = 1;
	*pAlong = t;
	return spline_distance2d( p, ImVec2( a.x + dx * t, a.y + dy * t ) );
}

// the nearest node on screen within radius, preferring the selected spline; skip leaves out one spline's nodes
static bool spline_picknode( ImVec2 mouse, float radius, int skipSpline, int skipNode, int* pSpline, int* pNode )
{
	float best = radius;
	bool bFound = false;
	for ( int si = 0; si < (int)g_Splines.size(); si++ )
	{
		const sSpline& s = g_Splines[ si ];
		for ( int ni = 0; ni < (int)s.nodes.size(); ni++ )
		{
			if ( si == skipSpline && ni == skipNode ) continue;
			ImVec2 p;
			if ( !spline_project( s.nodes[ni].x, spline_groundy( s.nodes[ni].x, s.nodes[ni].z ) + SPLINE_DRAW_LIFT, s.nodes[ni].z, &p ) ) continue;
			float d = spline_distance2d( mouse, p );
			if ( si == g_iSplineSelected ) d -= 1.0f;
			if ( d < best ) { best = d; *pSpline = si; *pNode = ni; bFound = true; }
		}
	}
	return bFound;
}

// the nearest handle of the selected Bezier spline within radius: 1 in, 2 out
// a node's handle (1 in, 2 out) is shown and can be dragged where its segment is Bezier
static bool spline_handleshown( const sSpline& s, int ni, int h )
{
	const int n = (int)s.nodes.size();
	if ( h == 1 ) return (ni > 0 || (s.closed && n > 2)) && spline_segcurve( s, spline_wrap( s, ni - 1 ) ) == SPLINE_CURVE_BEZIER;
	return ni < spline_segments( s ) && spline_segcurve( s, ni ) == SPLINE_CURVE_BEZIER;
}

static bool spline_pickhandle( ImVec2 mouse, float radius, int* pNode, int* pHandle )
{
	if ( g_iSplineSelected < 0 ) return false;
	const sSpline& s = g_Splines[ g_iSplineSelected ];
	float best = radius;
	bool bFound = false;
	for ( int ni = 0; ni < (int)s.nodes.size(); ni++ )
	{
		for ( int h = 1; h <= 2; h++ )
		{
			if ( !spline_handleshown( s, ni, h ) ) continue;
			const float hx = s.nodes[ni].x + (h == 1 ? s.nodes[ni].inX : s.nodes[ni].outX);
			const float hz = s.nodes[ni].z + (h == 1 ? s.nodes[ni].inZ : s.nodes[ni].outZ);
			ImVec2 p;
			if ( !spline_project( hx, spline_groundy( hx, hz ) + SPLINE_DRAW_LIFT, hz, &p ) ) continue;
			const float d = spline_distance2d( mouse, p );
			if ( d < best ) { best = d; *pNode = ni; *pHandle = h; bFound = true; }
		}
	}
	return bFound;
}

// the nearest point on a spline's curve on screen within radius (one spline, or every spline but skip when only < 0)
static bool spline_pickcurve( ImVec2 mouse, float radius, int only, int skip, int* pSpline, int* pSeg, float* pT )
{
	float best = radius;
	bool bFound = false;
	std::vector<sSplinePoint> points;
	for ( int si = 0; si < (int)g_Splines.size(); si++ )
	{
		if ( (only >= 0 && si != only) || si == skip ) continue;
		spline_sample( g_Splines[ si ], 100.0f, points );
		for ( size_t k = 1; k < points.size(); k++ )
		{
			const sSplinePoint& p0 = points[ k - 1 ];
			const sSplinePoint& p1 = points[ k ];
			ImVec2 a, b;
			if ( !spline_projectline( p0.x, p0.y + SPLINE_DRAW_LIFT, p0.z, p1.x, p1.y + SPLINE_DRAW_LIFT, p1.z, &a, &b ) ) continue;
			float along;
			const float d = spline_distancetosegment( mouse, a, b, &along );
			if ( d < best )
			{
				best = d;
				*pSpline = si;
				*pSeg = p0.seg;
				const float t1 = (p1.seg == p0.seg) ? p1.t : 1.0f;
				*pT = p0.t + (t1 - p0.t) * along;
				bFound = true;
			}
		}
	}
	return bFound;
}

static bool spline_terrainpick( float* pX, float* pY, float* pZ )
{
	return WickedCall_GetPick( pX, pY, pZ, NULL, NULL, NULL, NULL, GGRENDERLAYERS_TERRAIN );
}

static void spline_draw( void )
{
	ImGuiViewport* pViewport = ImGui::GetMainViewport();
	if ( !pViewport ) return;
	ImDrawList* dl = ImGui::GetForegroundDrawList( pViewport );
	if ( !dl ) return;
	dl->PushClipRect( renderTargetAreaPos, renderTargetAreaPos + renderTargetAreaSize );
	dl->AddCallback( (ImDrawCallback)10, NULL ); // force render

	std::vector<sSplinePoint> points;
	for ( int si = 0; si < (int)g_Splines.size(); si++ )
	{
		const sSpline& s = g_Splines[ si ];
		const bool bSelected = (si == g_iSplineSelected);
		const ImU32 lineCol = bSelected ? IM_COL32( 255, 205, 40, 255 ) : IM_COL32( 255, 205, 40, 120 );
		spline_sample( s, bSelected ? 150.0f : 300.0f, points );
		for ( size_t k = 1; k < points.size(); k++ )
		{
			ImVec2 a, b;
			if ( !spline_projectline( points[k-1].x, points[k-1].y + SPLINE_DRAW_LIFT, points[k-1].z, points[k].x, points[k].y + SPLINE_DRAW_LIFT, points[k].z, &a, &b ) ) continue;
			if ( bSelected && points[k-1].seg == g_iSplineSegSelected ) dl->AddLine( a, b, IM_COL32( 255, 255, 255, 255 ), 5.0f );
			else dl->AddLine( a, b, lineCol, bSelected ? 3.0f : 2.0f );
		}
		for ( int ni = 0; ni < (int)s.nodes.size(); ni++ )
		{
			const sSplineNode& node = s.nodes[ ni ];
			const float ny = spline_groundy( node.x, node.z ) + SPLINE_DRAW_LIFT;
			ImVec2 p;
			if ( !spline_project( node.x, ny, node.z, &p ) ) continue;
			if ( bSelected )
			{
				for ( int h = 1; h <= 2; h++ )
				{
					if ( !spline_handleshown( s, ni, h ) ) continue;
					const float hx = node.x + (h == 1 ? node.inX : node.outX);
					const float hz = node.z + (h == 1 ? node.inZ : node.outZ);
					ImVec2 a, b;
					if ( spline_projectline( node.x, ny, node.z, hx, spline_groundy( hx, hz ) + SPLINE_DRAW_LIFT, hz, &a, &b ) )
					{
						dl->AddLine( a, b, IM_COL32( 160, 220, 255, 200 ), 1.5f );
						dl->AddCircleFilled( b, 4.0f, IM_COL32( 160, 220, 255, 255 ) );
					}
				}
			}
			const bool bNodeSelected = bSelected && ni == g_iSplineNodeSelected;
			const float r = bNodeSelected ? 6.0f : 4.5f;
			ImU32 col = bSelected ? IM_COL32( 255, 240, 200, 255 ) : IM_COL32( 255, 220, 120, 160 );
			if ( node.junction ) col = IM_COL32( 80, 230, 255, 255 );
			dl->AddRectFilled( p - ImVec2( r, r ), p + ImVec2( r, r ), col );
			if ( bNodeSelected ) dl->AddRect( p - ImVec2( r + 3, r + 3 ), p + ImVec2( r + 3, r + 3 ), IM_COL32( 255, 255, 255, 255 ), 0.0f, 0, 2.0f );
		}
	}

	// where a dragged node will snap
	if ( iDragNode >= 0 && iSnapSpline >= 0 )
	{
		ImVec2 p;
		if ( spline_project( fSnapX, spline_groundy( fSnapX, fSnapZ ) + SPLINE_DRAW_LIFT, fSnapZ, &p ) )
			dl->AddCircle( p, 11.0f, IM_COL32( 80, 230, 255, 255 ), 20, 2.5f );
	}

	dl->AddCallback( (ImDrawCallback)11, NULL ); // disable force render
	dl->PopClipRect();
}

// where a node dragged to the mouse would snap: a node of another spline (or this spline's other end), else another
// spline's curve; never the nodes already joined to it, nor the curves of the splines joined there
static void spline_findsnap( ImVec2 mouse )
{
	iSnapSpline = -1;
	iSnapNode = -1;
	const sSplineNode& dragged = g_Splines[ iDragSpline ].nodes[ iDragNode ];
	auto joinedHere = [&dragged]( const sSpline& s )
	{
		if ( !dragged.junction ) return false;
		for ( const sSplineNode& node : s.nodes ) if ( node.junction == dragged.junction ) return true;
		return false;
	};

	float best = SPLINE_SNAP_NODE;
	for ( int si = 0; si < (int)g_Splines.size(); si++ )
	{
		const sSpline& s = g_Splines[ si ];
		for ( int ni = 0; ni < (int)s.nodes.size(); ni++ )
		{
			if ( si == iDragSpline && ni == iDragNode ) continue;
			const sSplineNode& node = s.nodes[ ni ];
			if ( dragged.junction && node.junction == dragged.junction ) continue;
			// an end onto the spline's other end closes it
			if ( si == iDragSpline && !(s.nodes.size() > 2 && !s.closed && spline_isend( s, iDragNode ) && spline_isend( s, ni )) ) continue;
			ImVec2 p;
			if ( !spline_project( node.x, spline_groundy( node.x, node.z ) + SPLINE_DRAW_LIFT, node.z, &p ) ) continue;
			const float d = spline_distance2d( mouse, p );
			if ( d < best )
			{
				best = d;
				iSnapSpline = si;
				iSnapNode = ni;
				fSnapX = node.x; fSnapY = node.y; fSnapZ = node.z;
			}
		}
	}
	if ( iSnapSpline >= 0 ) return;

	best = SPLINE_SNAP_CURVE;
	for ( int si = 0; si < (int)g_Splines.size(); si++ )
	{
		if ( si == iDragSpline || joinedHere( g_Splines[ si ] ) ) continue;
		int hit = -1, seg = -1;
		float t = 0;
		if ( !spline_pickcurve( mouse, best, si, -1, &hit, &seg, &t ) ) continue;
		float x, z;
		spline_point( g_Splines[ si ], seg, t, &x, &z );
		ImVec2 p;
		if ( spline_project( x, spline_groundy( x, z ) + SPLINE_DRAW_LIFT, z, &p ) ) best = spline_distance2d( mouse, p );
		iSnapSpline = si;
		iSnapNode = -1;
		iSnapSeg = seg;
		fSnapT = t;
		fSnapX = x; fSnapZ = z;
		fSnapY = spline_groundy( x, z );
	}
}

// a drag's start: where the mouse was, and the dragged node's (or handle's) offset from the ground under it, so a node
// clicked off its centre doesn't jump to the mouse (or snap onto one nearby), and nothing moves until the mouse does
static ImVec2 vDragStart;
static float fDragOffsetX = 0.0f, fDragOffsetZ = 0.0f;
static bool bDragMoved = false;
static void spline_dragstart( const ImVec2& mouse )
{
	vDragStart = mouse;
	bDragMoved = false;
	fDragOffsetX = fDragOffsetZ = 0.0f;
	float x, y, z;
	if ( iDragNode < 0 || !spline_terrainpick( &x, &y, &z ) ) return;
	const sSplineNode& node = g_Splines[ iDragSpline ].nodes[ iDragNode ];
	float targetX = node.x, targetZ = node.z;
	if ( iDragHandle == 1 ) { targetX += node.inX; targetZ += node.inZ; }
	else if ( iDragHandle == 2 ) { targetX += node.outX; targetZ += node.outZ; }
	fDragOffsetX = targetX - x;
	fDragOffsetZ = targetZ - z;
}

static void spline_mouse( void )
{
	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 mouse = io.MousePos;
	// ImGui's hovered window is the one under the mouse whichever window is drawn first this frame (the object library sets
	// bImGuiGotFocus only when it is drawn, after this)
	ImGuiWindow* pViewWindow = ImGui::FindWindowByName( TABEDITORNAME );
	ImGuiWindow* pHovered = GImGui->HoveredWindow;
	const bool bOverView = !pViewWindow || !pHovered || pHovered->RootWindow == pViewWindow->RootWindow;
	const bool bInView = bImGuiRenderTargetFocus && !bImGuiGotFocus && bOverView
		&& !ImGui::IsWindowHovered( ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem )
		&& mouse.x >= renderTargetAreaPos.x && mouse.y >= renderTargetAreaPos.y
		&& mouse.x < renderTargetAreaPos.x + renderTargetAreaSize.x && mouse.y < renderTargetAreaPos.y + renderTargetAreaSize.y;

	// a drag in progress follows the mouse until the button is let go, wherever the mouse is
	if ( iDragNode >= 0 && (iDragSpline >= (int)g_Splines.size() || iDragNode >= (int)g_Splines[ iDragSpline ].nodes.size()) )
	{
		iDragSpline = iDragNode = -1;
		iDragHandle = 0;
		iSnapSpline = iSnapNode = -1;
	}
	if ( iDragNode >= 0 )
	{
		sSpline& s = g_Splines[ iDragSpline ];
		float x, y, z;
		if ( io.MouseDown[0] && !bDragMoved && fabsf( mouse.x - vDragStart.x ) + fabsf( mouse.y - vDragStart.y ) >= 4.0f ) bDragMoved = true;
		if ( io.MouseDown[0] )
		{
			if ( bDragMoved && spline_terrainpick( &x, &y, &z ) )
			{
				x += fDragOffsetX;
				z += fDragOffsetZ;
				y = spline_groundy( x, z );
				if ( iDragHandle == 0 )
				{
					spline_findsnap( mouse );
					if ( iSnapSpline >= 0 ) { x = fSnapX; y = fSnapY; z = fSnapZ; }
					spline_movenode( iDragSpline, iDragNode, x, y, z );
				}
				else
				{
					sSplineNode& node = s.nodes[ iDragNode ];
					const float hx = x - node.x, hz = z - node.z;
					float* pThisX = (iDragHandle == 1) ? &node.inX : &node.outX;
					float* pThisZ = (iDragHandle == 1) ? &node.inZ : &node.outZ;
					float* pOtherX = (iDragHandle == 1) ? &node.outX : &node.inX;
					float* pOtherZ = (iDragHandle == 1) ? &node.outZ : &node.inZ;
					*pThisX = hx;
					*pThisZ = hz;
					if ( !(node.flags & SPLINE_NODE_BROKEN) )
					{
						const float len = sqrtf( hx * hx + hz * hz );
						float otherLen = sqrtf( (*pOtherX) * (*pOtherX) + (*pOtherZ) * (*pOtherZ) );
						if ( otherLen < 1.0f ) otherLen = len;
						if ( len > 0.001f ) { *pOtherX = -hx / len * otherLen; *pOtherZ = -hz / len * otherLen; }
					}
				}
				spline_modified();
			}
		}
		else
		{
			// let go: join the snap target
			if ( iDragHandle == 0 && iSnapSpline >= 0 )
			{
				if ( iSnapSpline == iDragSpline && iSnapNode >= 0 )
				{
					// an end on the other end: closed, the dragged end dropped
					s.closed = 1;
					s.nodes.erase( s.nodes.begin() + iDragNode );
					spline_cleanjunctions();
					g_iSplineNodeSelected = -1;
				}
				else
				{
					int sj = iSnapSpline, nj = iSnapNode;
					if ( nj < 0 ) nj = spline_insertnode( sj, iSnapSeg, fSnapT );
					spline_join( iDragSpline, iDragNode, sj, nj );
				}
				spline_modified();
			}
			iDragSpline = iDragNode = -1;
			iDragHandle = 0;
			iSnapSpline = iSnapNode = -1;
		}
		return;
	}

	if ( !bInView || !io.MouseClicked[0] ) return;

	int si = -1, ni = -1, h = 0, seg = -1;
	float t = 0;

	// Connect mode: an end node of another spline joins this one with a new segment
	if ( bConnectMode )
	{
		bConnectMode = false;
		if ( g_iSplineSelected >= 0 && g_iSplineNodeSelected >= 0 && spline_picknode( ImVec2( mouse ), SPLINE_PICK_NODE, -1, -1, &si, &ni ) )
		{
			if ( si != g_iSplineSelected && spline_isend( g_Splines[ si ], ni ) && spline_isend( g_Splines[ g_iSplineSelected ], g_iSplineNodeSelected ) )
				spline_merge( g_iSplineSelected, g_iSplineNodeSelected, si, ni, false );
		}
		return;
	}

	// drawing (a node just added at an end of the selected spline): a click on another spline's node adds the next node
	// there joined to it, on another spline's curve too (a node is inserted on it), on this spline's other end closes it
	if ( bDrawing && g_iSplineSelected >= 0 && !io.KeyCtrl && !io.KeyAlt && !io.KeyShift )
	{
		const int sel = g_iSplineSelected;
		sSpline& s = g_Splines[ sel ];
		const bool bAtEnd = s.nodes.empty() || (g_iSplineNodeSelected >= 0 && spline_isend( s, g_iSplineNodeSelected ));
		if ( bAtEnd && !s.closed )
		{
			if ( spline_picknode( mouse, SPLINE_PICK_NODE, -1, -1, &si, &ni ) )
			{
				if ( si != sel )
				{
					const sSplineNode target = g_Splines[ si ].nodes[ ni ];
					const int at = spline_addend( sel, target.x, target.y, target.z );
					spline_join( sel, at, si, ni );
					g_iSplineNodeSelected = at;
					g_iSplineSegSelected = -1;
					return;
				}
				if ( s.nodes.size() > 2 && spline_isend( s, ni ) && ni != g_iSplineNodeSelected )
				{
					s.closed = 1;
					bDrawing = false;
					g_iSplineNodeSelected = -1;
					spline_modified();
					return;
				}
			}
			else if ( spline_pickcurve( mouse, SPLINE_PICK_CURVE, -1, sel, &si, &seg, &t ) )
			{
				const int nj = spline_insertnode( si, seg, t );
				const sSplineNode target = g_Splines[ si ].nodes[ nj ];
				const int at = spline_addend( sel, target.x, target.y, target.z );
				spline_join( sel, at, si, nj );
				g_iSplineNodeSelected = at;
				g_iSplineSegSelected = -1;
				return;
			}
		}
	}

	// a handle of the selected spline
	if ( spline_pickhandle( mouse, SPLINE_PICK_NODE, &ni, &h ) )
	{
		g_iSplineNodeSelected = ni;
		iDragSpline = g_iSplineSelected;
		iDragNode = ni;
		iDragHandle = h;
		if ( io.KeyAlt ) g_Splines[ iDragSpline ].nodes[ ni ].flags |= SPLINE_NODE_BROKEN;
		spline_dragstart( mouse );
		return;
	}

	// a node: Ctrl+click deletes it, Alt pulls it out of its junction, else it is dragged
	if ( spline_picknode( mouse, SPLINE_PICK_NODE, -1, -1, &si, &ni ) )
	{
		g_iSplineSelected = si;
		bDrawing = false;
		if ( io.KeyCtrl )
		{
			spline_deletenode( si, ni );
			return;
		}
		if ( io.KeyAlt && g_Splines[ si ].nodes[ ni ].junction )
		{
			g_Splines[ si ].nodes[ ni ].junction = 0;
			spline_cleanjunctions();
		}
		g_iSplineNodeSelected = ni;
		g_iSplineSegSelected = -1;
		iDragSpline = si;
		iDragNode = ni;
		iDragHandle = 0;
		spline_dragstart( mouse );
		return;
	}

	// Shift+click on the selected spline's curve inserts a node there
	if ( io.KeyShift && g_iSplineSelected >= 0 && spline_pickcurve( mouse, SPLINE_PICK_CURVE, g_iSplineSelected, -1, &si, &seg, &t ) )
	{
		bDrawing = false;
		g_iSplineNodeSelected = spline_insertnode( si, seg, t );
		iDragSpline = si;
		iDragNode = g_iSplineNodeSelected;
		iDragHandle = 0;
		spline_dragstart( mouse );
		return;
	}

	// a click on a spline's curve selects it, and on the selected spline the segment between two nodes
	if ( spline_pickcurve( mouse, SPLINE_PICK_CURVE, -1, -1, &si, &seg, &t ) )
	{
		if ( si == g_iSplineSelected ) g_iSplineSegSelected = seg;
		else g_iSplineSegSelected = -1;
		g_iSplineSelected = si;
		g_iSplineNodeSelected = -1;
		bDrawing = false;
		return;
	}

	// the terrain: a new node at the end of the selected spline (or its start, with the first node selected), dragged while held
	float x, y, z;
	if ( !spline_terrainpick( &x, &y, &z ) ) return;
	if ( g_iSplineSelected < 0 ) g_iSplineSelected = spline_new();
	sSpline& s = g_Splines[ g_iSplineSelected ];
	if ( s.closed ) return;
	const int at = spline_addend( g_iSplineSelected, x, y, z );
	bDrawing = true;
	g_iSplineNodeSelected = at;
	g_iSplineSegSelected = -1;
	iDragSpline = g_iSplineSelected;
	iDragNode = at;
	iDragHandle = 0;
	spline_dragstart( mouse );
	spline_modified();
}

//
// The panel (Terrain Tools) and the editing in the 3D view
//

bool spline_iseditmode( void )
{
	return g_bSplineEditMode;
}

// the object library is picking an entity for a placement layer (while the panel is shown)
bool spline_librarypicking( void )
{
	return g_bLibraryPickEntity && ImGui::GetFrameCount() - g_iSplinePanelFrame <= 2;
}

static float fRowLabelX = 0, fRowFieldX = 0, fRowRight = 0;

// a row: its label, then the next item from the field column to the right edge
static void spline_row( const char* pLabel )
{
	ImGui::SetCursorPosX( fRowLabelX );
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted( pLabel );
	ImGui::SameLine();
	ImGui::SetCursorPosX( fRowFieldX );
	ImGui::SetNextItemWidth( fRowRight - fRowFieldX );
}

// a length in units shown in metres
static bool spline_rowmetres( const char* pLabel, const char* pId, float* pUnits, float minM, float maxM, const char* pFormat = "%.1f m" )
{
	spline_row( pLabel );
	float m = *pUnits / SPLINE_UNITS_PER_M;
	if ( !ImGui::SliderFloat( pId, &m, minM, maxM, pFormat ) ) return false;
	*pUnits = m * SPLINE_UNITS_PER_M;
	return true;
}

// the Paint palette's entry iL: its picture (loaded here as the palette loads it, if the palette hasn't been shown yet) and
// the texture slot it paints; false for an empty entry
static bool spline_paletteentry( int iL, int* pImage, int* pSlot )
{
	if ( t.visuals.sTerrainTextures[ iL ] == "" ) return false;
	const bool bPaletteShown = sTerrainTexturesID[ 0 ] > 0;
	const int image = bPaletteShown ? sTerrainTexturesID[ iL ] : t.terrain.imagestartindex + 80 + iL;
	*pSlot = bPaletteShown ? sTerrainSelectionID[ iL ] : iL;
	if ( image <= 0 ) return false;
	// a texture whose picture won't load is tried once, not every frame (each try reads the whole file), until its path changes
	static std::string failed[ 32 ];
	if ( ImageExist( image ) == 0 && failed[ iL ] == t.visuals.sTerrainTextures[ iL ].Get() ) return false;
	if ( ImageExist( image ) == 0 )
	{
		image_setlegacyimageloading( true );
		SetMipmapNum( 1 );
		// the compressed version of an "(uncompressed)" texture loads quicker
		char path[ MAX_PATH ];
		strcpy_s( path, MAX_PATH, t.visuals.sTerrainTextures[ iL ].Get() );
		const int len = (int)strlen( path ) - (int)strlen( " (uncompressed).dds" );
		if ( len > 0 )
		{
			path[ len ] = 0;
			strcat_s( path, MAX_PATH, ".dds" );
			if ( FileExist( path ) == 0 ) strcpy_s( path, MAX_PATH, t.visuals.sTerrainTextures[ iL ].Get() );
		}
		LoadImage( path, image, 0, g.gdividetexturesize );
		SetMipmapNum( -1 );
		image_setlegacyimageloading( false );
	}
	*pImage = image;
	if ( ImageExist( image ) == 0 )
	{
		failed[ iL ] = t.visuals.sTerrainTextures[ iL ].Get();
		return false;
	}
	failed[ iL ].clear();
	return true;
}

// a terrain texture slot (stored + 1): the texture's picture, which opens the palette's textures as a grid to pick from
static void spline_rowtexture( const char* pLabel, const char* pId, int* pSlot, float w )
{
	const float size = ImGui::GetFontSize() * 2.6f;
	const float cell = ImGui::GetFontSize() * 4.0f;
	int current = -1, currentImage = 0;
	for ( int iL = 0; iL < 32; iL++ )
	{
		int image = 0, slot = 0;
		if ( spline_paletteentry( iL, &image, &slot ) && slot == *pSlot - 1 ) { current = iL; currentImage = image; break; }
	}

	ImGui::SetCursorPosX( fRowLabelX );
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted( pLabel );
	ImGui::SameLine();
	ImGui::SetCursorPosX( fRowFieldX );
	char popup[ 64 ];
	sprintf_s( popup, 64, "Textures%s", pId );
	ImGui::PushID( pId );
	bool bOpen = false;
	if ( current >= 0 )
	{
		ImGui::SetBlurMode( true );
		bOpen = ImGui::ImgBtn( currentImage, ImVec2( size, size ), ImColor( 0, 0, 0, 255 ), ImColor( 220, 220, 220, 220 ), ImColor( 255, 255, 255, 255 ), ImColor( 180, 180, 160, 255 ), -1, 0, 0, 0, false, false, false, false, false, true );
		ImGui::SetBlurMode( false );
	}
	else
	{
		bOpen = ImGui::StyleButton( "None##picker", ImVec2( size, size ) );
	}
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Click to pick a texture" );
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted( current >= 0 ? t.visuals.sTerrainTexturesName[ current ].Get() : "None" );
	if ( bOpen ) ImGui::OpenPopup( popup );
	if ( ImGui::BeginPopup( popup ) )
	{
		if ( ImGui::StyleButton( "None##pickernone", ImVec2( cell * 4.0f + 18.0f, 0 ) ) ) { *pSlot = 0; ImGui::CloseCurrentPopup(); }
		if ( ImGui::StyleButton( "The Paint Tool's Texture##pickerpaint", ImVec2( cell * 4.0f + 18.0f, 0 ) ) ) { *pSlot = iCurrentTextureForPaint + 1; ImGui::CloseCurrentPopup(); }
		int column = 0;
		ImGui::SetBlurMode( true );
		for ( int iL = 0; iL < 32; iL++ )
		{
			int image = 0, slot = 0;
			if ( !spline_paletteentry( iL, &image, &slot ) ) continue;
			ImGui::PushID( iL );
			if ( ImGui::ImgBtn( image, ImVec2( cell, cell ), ImColor( 0, 0, 0, 255 ), ImColor( 220, 220, 220, 220 ), ImColor( 255, 255, 255, 255 ), ImColor( 180, 180, 160, 255 ), -1, 0, 0, 0, false, false, false, false, false, true ) )
			{
				*pSlot = slot + 1;
				ImGui::CloseCurrentPopup();
			}
			if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", t.visuals.sTerrainTexturesName[ iL ].Get() );
			ImGui::PopID();
			if ( ++column % 4 ) ImGui::SameLine();
		}
		ImGui::SetBlurMode( false );
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

// the placement layers of a road or a river
static void spline_rowlayers( sSpline& s, float w )
{
	ImGui::SetCursorPosX( fRowLabelX );
	if ( !ImGui::TreeNodeEx( "Placement Layers##splinelayers", ImGuiTreeNodeFlags_DefaultOpen ) ) return;
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Entities placed along it at a spacing: streetlights, rocks, posts, anything. Placed again when it changes; one you move by hand stays" );
	int remove = -1;
	for ( int li = 0; li < (int)s.layers.size(); li++ )
	{
		sSplineLayer& layer = s.layers[ li ];
		int placedHere = 0;
		for ( const sSplinePlaced& placed : s.placed ) if ( placed.layer == li ) placedHere++;
		const char* pName = strrchr( layer.entity, '\\' );
		pName = pName ? pName + 1 : (layer.entity[0] ? layer.entity : "no entity");
		char label[ 400 ];
		sprintf_s( label, 400, "%d: %s (%d placed)###splinelayer%d", li + 1, pName, placedHere, li );
		ImGui::SetCursorPosX( fRowLabelX );
		if ( !ImGui::TreeNodeEx( label, 0 ) ) continue;
		ImGui::PushID( li );
		bool bOn = layer.enabled != 0;
		ImGui::SetCursorPosX( fRowFieldX );
		if ( ImGui::Checkbox( "On##layeron", &bOn ) ) layer.enabled = bOn ? 1 : 0;
		bool bFrozen = layer.frozen != 0;
		ImGui::SameLine();
		if ( ImGui::Checkbox( "Freeze##layerfrozen", &bFrozen ) ) layer.frozen = bFrozen ? 1 : 0;
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Keep this layer's entities as they are: they are no longer placed again when the road or the layer changes, so you can edit them one by one" );
		spline_row( "Entity" );
		ImGui::InputText( "##layerentity", layer.entity, 260 );
		// the object library, picking: the clicked entity comes back here
		const ImGuiID pickID = ImGui::GetID( "##layerlibrarypick" );
		if ( iSelectedLibraryStingReturnID == (int)pickID )
		{
			strcpy_s( layer.entity, 260, sSelectedLibrarySting.Get() );
			iSelectedLibraryStingReturnID = -1;
			sSelectedLibrarySting = "";
			g_bLibraryPickEntity = false;
			spline_modified();
		}
		ImGui::SetCursorPosX( fRowFieldX );
		const float half = (fRowRight - fRowFieldX) * 0.5f - 2.0f;
		if ( ImGui::StyleButton( "Library...##layerlibrary", ImVec2( half, 0 ) ) )
		{
			bExternal_Entities_Window = true;
			iDisplayLibraryType = 0;
			iDisplayLibrarySubType = 0;
			iLibraryStingReturnToID = (int)pickID;
			g_bLibraryPickEntity = true;
		}
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Pick the entity in the object library: click one and it comes back here" );
		ImGui::SameLine();
		if ( ImGui::StyleButton( "File...##layerchoose", ImVec2( half, 0 ) ) )
		{
			char pStart[ MAX_PATH ] = "entitybank\\";
			GG_GetRealPath( pStart, 0 );
			char pFull[ MAX_PATH ];
			if ( !_fullpath( pFull, pStart, MAX_PATH ) ) strcpy_s( pFull, MAX_PATH, pStart );
			const char* pChosen = noc_file_dialog_open( NOC_FILE_DIALOG_OPEN, "Entity\0*.fpe\0", pFull, NULL, true, "Choose an Entity" );
			if ( pChosen )
			{
				char pLower[ MAX_PATH ];
				strcpy_s( pLower, MAX_PATH, pChosen );
				_strlwr_s( pLower, MAX_PATH );
				const char* pBank = strstr( pLower, "entitybank\\" );
				if ( pBank ) strcpy_s( layer.entity, 260, pChosen + (pBank - pLower) + strlen( "entitybank\\" ) );
			}
		}
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Choose an entity file (.fpe) in the entity bank" );
		spline_row( "Name" );
		const char* pDesc = spline_entitydesc( layer.entity );
		ImGui::InputTextWithHint( "##layername", pDesc[0] ? pDesc : "the entity's name", layer.name, 64 );
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Each placed one is named the entity's own name (shown greyed), or this if set, with __01, __02 ... after it, numbered on from the highest of that name already in the level" );
		spline_rowmetres( "Spacing", "##layerspacing", &layer.spacing, 1.0f, 200.0f );
		spline_rowmetres( "Start", "##layerstart", &layer.start, 0.0f, 200.0f );
		const char* sides[] = { "Both", "Left", "Right", "Alternate", "Centre" };
		spline_row( "Side" );
		ImGui::Combo( "##layerside", &layer.side, sides, 5 );
		spline_rowmetres( layer.side == SPLINE_SIDE_CENTRE ? "Across" : "From the Edge", "##layeroffset", &layer.offset, -20.0f, 40.0f );
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", layer.side == SPLINE_SIDE_CENTRE ? "How far right of the centre (left if less than 0)" : "How far out from the road's or the river bed's edge (in if less than 0)" );
		const char* faces[] = { "Mirrored per Side", "Along", "Random" };
		spline_row( "Facing" );
		ImGui::Combo( "##layerfacing", &layer.facing, faces, 3 );
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Mirrored per Side: the right side faces along the spline, the left side back (a lamp's arm reaches over the road from both sides; cars on Both sides, From the Edge less than 0, drive on the right).\nAlong: all face along it. Random: any way." );
		spline_row( "Extra Turn" );
		ImGui::SliderFloat( "##layerturn", &layer.turn, -180.0f, 180.0f, "%.0f deg" );
		spline_rowmetres( "Height", "##layerheight", &layer.height, -5.0f, 10.0f, "%.2f m" );
		bool bSlope = layer.followSlope != 0;
		ImGui::SetCursorPosX( fRowFieldX );
		if ( ImGui::Checkbox( "Follow Slope##layerslope", &bSlope ) ) layer.followSlope = bSlope ? 1 : 0;
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Tilt each to the ground under it (decals, drain covers, flat things); off keeps them upright (lamps, posts)" );
		const char* flats[] = { "No (as Modelled)", "Yes", "Yes, Flipped" };
		spline_row( "Lay Flat" );
		ImGui::Combo( "##layerlayflat", &layer.layFlat, flats, 3 );
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "For a model that stands up (a wall decal): turned onto its back to lie on the ground, before the slope's tilt.\nFlipped if it lies face down (you see it from below only)." );
		spline_row( "Scale" );
		float scale[2] = { layer.scaleMin, layer.scaleMax };
		if ( ImGui::SliderFloat2( "##layerscale", scale, 10.0f, 400.0f, "%.0f %%" ) ) { layer.scaleMin = std::min( scale[0], scale[1] ); layer.scaleMax = std::max( scale[0], scale[1] ); }
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Each is scaled at random between the two" );
		spline_rowmetres( "Jitter Along", "##layerjitter", &layer.jitter, 0.0f, 20.0f );
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Each moved up to this far along the spline, either way, at random" );
		spline_rowmetres( "Jitter Across", "##layerjitteracross", &layer.jitterAcross, 0.0f, 20.0f );
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Each moved up to this far across it, either side, at random (with Side Centre and half the road's width, spread over both lanes)" );
		spline_rowmetres( "Keep Apart", "##layerkeepapart", &layer.keepApart, 0.0f, 100.0f );
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "None placed this close to one of the same entity another spline placed (where roads meet or run side by side); 0 to allow any" );
		if ( s.kind == SPLINE_KIND_RIVER )
		{
			spline_row( "Only in Rapids" );
			ImGui::SliderFloat( "##layerrapids", &layer.minTurbulence, 0.0f, 1.0f, "%.2f" );
			if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Only where the water is at least this turbulent (0 anywhere): rocks in the rapids" );
		}
		ImGui::SetCursorPosX( fRowFieldX );
		if ( ImGui::StyleButton( "Remove Layer##layerremove", ImVec2( fRowRight - fRowFieldX, 0 ) ) ) remove = li;
		ImGui::PopID();
		ImGui::TreePop();
	}
	if ( remove >= 0 )
	{
		// its entities go with it (a frozen layer's stay as ordinary entities)
		std::vector<sSplinePlaced> others;
		for ( const sSplinePlaced& placed : s.placed ) if ( placed.layer != remove ) others.push_back( placed );
		if ( !s.layers[ remove ].frozen )
		{
			std::vector<sSplinePlaced> mine;
			for ( const sSplinePlaced& placed : s.placed ) if ( placed.layer == remove ) mine.push_back( placed );
			std::vector<sSplinePlaced> all = s.placed;
			s.placed = mine;
			spline_unplace( s );
			s.placed = all;
		}
		s.placed.clear();
		for ( sSplinePlaced placed : others )
		{
			if ( placed.layer > remove ) placed.layer--;
			s.placed.push_back( placed );
		}
		s.layers.erase( s.layers.begin() + remove );
		spline_modified();
	}
	ImGui::SetCursorPosX( fRowLabelX );
	if ( ImGui::StyleButton( "Add Layer##splineaddlayer", ImVec2( w * 0.45f, 0 ) ) )
	{
		s.layers.push_back( spline_newlayer( s ) );
		spline_modified();
	}
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", s.kind == SPLINE_KIND_ROAD ? "A new layer of streetlights (the Long Bien bridge's, 30.8 m apart on both sides); change its entity and spacing as you like" : "A new layer for rocks or plants along the banks: choose its entity" );
	ImGui::TreePop();
}

//
// Presets
//

// a road's or a river's preset (Roads and Rivers' Preset list): its settings, textures and layers, in a form any level's
// paint palette and entity bank can take. Built in, and the user's own in editors\splinepresets.txt (Save as Preset)
struct sSplinePreset
{
	std::string name;
	int kind = SPLINE_KIND_ROAD;
	bool bUser = false;
	sSplineRoad road;
	sSplineRiver river;
	std::string textures[2]; // a road's carriageway and shoulders, a river's bed and banks: candidates
	std::vector<sSplineLayer> layers;
	std::vector<std::string> entities; // each layer's entity: candidates
};
static std::vector<sSplinePreset> g_SplinePresets; // the built-in ones, the user's in place of one of the same name, and the user's others
static std::vector<sSplinePreset> g_SplineBuiltInPresets;
static std::vector<sSplinePreset> g_SplineUserPresets;
static bool g_bSplinePresetsLoaded = false;
#define SPLINE_PRESET_FILE "editors\\splinepresets.txt"

static const char* g_pSplineBuiltInPresets =
	"[road: Footpath]\n"
	"width = 1.5\n"
	"shoulder = 0.8\n"
	"smoothing = 6\n"
	"maxgrade = 35\n"
	"crown = 0\n"
	"grassmargin = 0.3\n"
	"treemargin = 0.6\n"
	"texture = mat28, kind:dirt\n"
	"edgetexture = none\n"
	"\n"
	"[road: Jungle Trail]\n"
	"width = 2.5\n"
	"shoulder = 1.2\n"
	"smoothing = 12\n"
	"maxgrade = 25\n"
	"crown = 0\n"
	"grassmargin = 0.5\n"
	"treemargin = 1.2\n"
	"texture = mat10, kind:dirt\n"
	"edgetexture = none\n"
	"layer = Puddles\n"
	"entity = Max Collection\\Cellar\\Small Puddle.fpe | find:puddle\n"
	"spacing = 30\n"
	"start = 10\n"
	"side = centre\n"
	"offset = 0\n"
	"facing = random\n"
	"height = 0.02\n"
	"scale = 60, 110\n"
	"jitteralong = 12\n"
	"jitteracross = 0.6\n"
	"followslope = 1\n"
	"\n"
	"[road: Dirt Track]\n"
	"width = 3.5\n"
	"shoulder = 1.5\n"
	"smoothing = 20\n"
	"maxgrade = 18\n"
	"crown = 0.03\n"
	"grassmargin = 0.6\n"
	"treemargin = 1.5\n"
	"texture = mat28, kind:dirt\n"
	"edgetexture = none\n"
	"layer = Puddles\n"
	"entity = Max Collection\\Cellar\\Small Puddle.fpe | find:puddle\n"
	"spacing = 45\n"
	"start = 15\n"
	"side = centre\n"
	"offset = 0\n"
	"facing = random\n"
	"height = 0.02\n"
	"scale = 60, 120\n"
	"jitteralong = 18\n"
	"jitteracross = 1\n"
	"followslope = 1\n"
	"\n"
	"[road: Jungle Road]\n"
	"width = 5\n"
	"shoulder = 2\n"
	"smoothing = 30\n"
	"maxgrade = 15\n"
	"crown = 0.05\n"
	"grassmargin = 1.2\n"
	"treemargin = 3\n"
	"texture = mat15, mat16, kind:dirt\n"
	"edgetexture = mat10, kind:dirt\n"
	"layer = Puddles\n"
	"entity = Max Collection\\Cellar\\Puddle.fpe | find:puddle\n"
	"spacing = 40\n"
	"start = 12\n"
	"side = centre\n"
	"offset = 0\n"
	"facing = random\n"
	"height = 0.02\n"
	"scale = 70, 130\n"
	"jitteralong = 15\n"
	"jitteracross = 1.5\n"
	"followslope = 1\n"
	"\n"
	"[road: Gravel Road]\n"
	"width = 5\n"
	"shoulder = 1.5\n"
	"smoothing = 40\n"
	"maxgrade = 12\n"
	"crown = 0.08\n"
	"grassmargin = 0.8\n"
	"treemargin = 2\n"
	"texture = gravel, mat31, mat23, kind:stone\n"
	"edgetexture = mat28, kind:dirt\n"
	"\n"
	"[road: Country Road]\n"
	"width = 6\n"
	"shoulder = 1.5\n"
	"smoothing = 60\n"
	"maxgrade = 10\n"
	"crown = 0.1\n"
	"grassmargin = 1\n"
	"treemargin = 2.5\n"
	"texture = asphalt, tarmac, mat23, kind:stone\n"
	"edgetexture = gravel, mat31, kind:stone\n"
	"centreline = dashed\n"
	"dash = 2\n"
	"gap = 6\n"
	"wear = 0.5\n"
	"layer = Patches\n"
	"entity = find:pothole | find:roadpatch | find:asphaltpatch | Basement Collection\\Decals\\Concrete - Patch 1.fpe\n"
	"spacing = 55\n"
	"start = 20\n"
	"side = centre\n"
	"offset = 0\n"
	"facing = random\n"
	"height = 0.01\n"
	"scale = 70, 130\n"
	"jitteralong = 25\n"
	"jitteracross = 1.8\n"
	"followslope = 1\n"
	"layflat = 1\n"
	"\n"
	"[road: Main Road]\n"
	"width = 8\n"
	"shoulder = 2\n"
	"smoothing = 80\n"
	"maxgrade = 8\n"
	"crown = 0.12\n"
	"grassmargin = 1.5\n"
	"treemargin = 3\n"
	"texture = asphalt, tarmac, mat23, kind:stone\n"
	"edgetexture = gravel, mat31, kind:stone\n"
	"centreline = dashed\n"
	"dash = 3\n"
	"gap = 9\n"
	"solidonbends = 150\n"
	"edgelines = 1\n"
	"edgeinset = 0.2\n"
	"wear = 0.15\n"
	"layer = Streetlights\n"
	"entity = find:streetlight | find:streetlamp | find:lamppost\n"
	"spacing = 32\n"
	"side = both\n"
	"offset = 0.5\n"
	"facing = mirrored\n"
	"keepapart = 15\n"
	"layer = Patches\n"
	"entity = find:pothole | find:roadpatch | find:asphaltpatch | Basement Collection\\Decals\\Concrete - Patch 1.fpe\n"
	"spacing = 70\n"
	"start = 25\n"
	"side = centre\n"
	"offset = 0\n"
	"facing = random\n"
	"height = 0.01\n"
	"scale = 70, 130\n"
	"jitteralong = 30\n"
	"jitteracross = 2.5\n"
	"followslope = 1\n"
	"layflat = 1\n"
	"\n"
	"[road: Town Street]\n"
	"width = 7\n"
	"shoulder = 1.5\n"
	"smoothing = 40\n"
	"maxgrade = 10\n"
	"crown = 0.1\n"
	"grassmargin = 2.5\n"
	"treemargin = 4\n"
	"texture = asphalt, tarmac, mat23, kind:stone\n"
	"edgetexture = pavement, concrete, cobble, mat18, kind:stone\n"
	"centreline = dashed\n"
	"dash = 2\n"
	"gap = 6\n"
	"wear = 0.25\n"
	"layer = Streetlights\n"
	"entity = find:streetlight | find:streetlamp | find:lamppost\n"
	"spacing = 25\n"
	"side = both\n"
	"offset = 0.3\n"
	"facing = mirrored\n"
	"keepapart = 12\n"
	"\n"
	"[road: Highway]\n"
	"width = 14\n"
	"shoulder = 3\n"
	"smoothing = 150\n"
	"maxgrade = 6\n"
	"crown = 0.15\n"
	"grassmargin = 2\n"
	"treemargin = 5\n"
	"texture = asphalt, tarmac, mat23, kind:stone\n"
	"edgetexture = gravel, mat31, kind:stone\n"
	"centreline = double\n"
	"lanes = 2\n"
	"dash = 3\n"
	"gap = 9\n"
	"edgelines = 1\n"
	"edgeinset = 0.2\n"
	"wear = 0.1\n"
	"layer = Streetlights\n"
	"entity = find:streetlight | find:streetlamp | find:lamppost\n"
	"spacing = 45\n"
	"side = both\n"
	"offset = 1\n"
	"facing = mirrored\n"
	"keepapart = 20\n"
	"\n"
	"[road: Airstrip]\n"
	"width = 30\n"
	"shoulder = 6\n"
	"smoothing = 400\n"
	"maxgrade = 2\n"
	"crown = 0.1\n"
	"grassmargin = 3\n"
	"treemargin = 20\n"
	"texture = asphalt, tarmac, concrete, mat23, kind:stone\n"
	"edgetexture = gravel, mat31, kind:stone\n"
	"\n"
	"[river: Creek]\n"
	"bedwidth = 1.5\n"
	"depth = 0.8\n"
	"banks = 2\n"
	"smoothing = 12\n"
	"grassmargin = 0.5\n"
	"treemargin = 1\n"
	"bedtexture = mat22, kind:stone\n"
	"banktexture = mat10, kind:dirt\n"
	"waterdepth = 0.35\n"
	"rapids = 1\n"
	"steepflow = 1.5\n"
	"bankfoam = 0.2\n"
	"layer = Rocks in Rapids\n"
	"entity = Max Collection\\Rocks\\Rock Small Round.fpe | find:rock\n"
	"spacing = 5\n"
	"side = centre\n"
	"offset = 0\n"
	"facing = random\n"
	"height = -0.1\n"
	"scale = 30, 80\n"
	"jitteralong = 2\n"
	"jitteracross = 0.6\n"
	"onlyinrapids = 0.4\n"
	"layer = Bank Rocks\n"
	"entity = Max Collection\\Rocks\\Rock Small Group.fpe | find:rock\n"
	"spacing = 7\n"
	"side = both\n"
	"offset = 0.2\n"
	"facing = random\n"
	"height = -0.1\n"
	"scale = 40, 90\n"
	"jitteralong = 3\n"
	"jitteracross = 0.5\n"
	"\n"
	"[river: Stream]\n"
	"bedwidth = 4\n"
	"depth = 1.5\n"
	"banks = 3.5\n"
	"smoothing = 25\n"
	"grassmargin = 1\n"
	"treemargin = 2\n"
	"bedtexture = mat22, mat23, kind:stone\n"
	"banktexture = mat10, kind:dirt\n"
	"waterdepth = 0.8\n"
	"rapids = 1\n"
	"steepflow = 1.5\n"
	"bankfoam = 0.25\n"
	"layer = Rocks in Rapids\n"
	"entity = Max Collection\\Rocks\\Rock Small Irregular.fpe | find:rock\n"
	"spacing = 6\n"
	"side = centre\n"
	"offset = 0\n"
	"facing = random\n"
	"height = -0.15\n"
	"scale = 50, 120\n"
	"jitteralong = 3\n"
	"jitteracross = 1.5\n"
	"onlyinrapids = 0.4\n"
	"layer = Bank Rocks\n"
	"entity = Max Collection\\Rocks\\Rock Small Group.fpe | find:rock\n"
	"spacing = 10\n"
	"side = both\n"
	"offset = 0.3\n"
	"facing = random\n"
	"height = -0.15\n"
	"scale = 50, 110\n"
	"jitteralong = 4\n"
	"jitteracross = 0.8\n"
	"\n"
	"[river: River]\n"
	"bedwidth = 15\n"
	"depth = 3\n"
	"banks = 8\n"
	"smoothing = 60\n"
	"grassmargin = 2\n"
	"treemargin = 3\n"
	"bedtexture = mat10, kind:dirt\n"
	"banktexture = mat28, kind:dirt\n"
	"waterdepth = 2.2\n"
	"rapids = 0.8\n"
	"steepflow = 1.5\n"
	"bankfoam = 0.3\n"
	"layer = Boulders in Rapids\n"
	"entity = Max Collection\\Rocks\\Rock Boulder.fpe | find:boulder | find:rock\n"
	"spacing = 12\n"
	"side = centre\n"
	"offset = 0\n"
	"facing = random\n"
	"height = -0.3\n"
	"scale = 60, 150\n"
	"jitteralong = 5\n"
	"jitteracross = 6\n"
	"onlyinrapids = 0.5\n"
	"\n"
	"[river: Wide River]\n"
	"bedwidth = 40\n"
	"depth = 4.5\n"
	"banks = 15\n"
	"smoothing = 120\n"
	"grassmargin = 3\n"
	"treemargin = 5\n"
	"bedtexture = mat10, kind:dirt\n"
	"banktexture = mat26, kind:sand, kind:dirt\n"
	"waterdepth = 3.2\n"
	"rapids = 0.5\n"
	"steepflow = 1.2\n"
	"bankfoam = 0.3\n"
	"\n"
	"[river: Irrigation Canal]\n"
	"bedwidth = 2.5\n"
	"depth = 1.2\n"
	"banks = 1.2\n"
	"smoothing = 60\n"
	"grassmargin = 0.5\n"
	"treemargin = 1\n"
	"bedtexture = mat10, kind:dirt\n"
	"banktexture = mat10, kind:dirt\n"
	"waterdepth = 0.8\n"
	"rapids = 0\n"
	"steepflow = 0.5\n"
	"bankfoam = 0.05\n";

static const char* g_pSplinePresetFileHeader =
	"; Roads and Rivers presets of your own (Terrain Tools, Roads and Rivers, Preset). Save as Preset writes this file, and it\n"
	"; can be edited by hand too (comments are not kept). One here with a built-in preset's name replaces it for you.\n"
	"; [road: Name] or [river: Name] starts a preset. Lengths are in metres, grades and scales in percent.\n"
	"; Textures: candidates in order, the first the level's paint palette has: matN (a stock texture, terraintextures\\matN),\n"
	";   kind:dirt (a material type: grass, stone, metal, wood, snow, dirt or sand), any part of a texture's name, or none.\n"
	"; layer = <label> starts a layer. Its entity: candidates separated by |, the first installed: a path in the entity bank,\n"
	";   or find:<words> (the first entity file whose name holds the words, spaces, _ and - ignored, your own first).\n"
	"; side: both, left, right, alternate or centre. facing: mirrored (the right side along, the left back), along or random.\n";

static std::string spline_trim( const std::string& text )
{
	const size_t a = text.find_first_not_of( " \t\r\n" );
	if ( a == std::string::npos ) return "";
	const size_t b = text.find_last_not_of( " \t\r\n" );
	return text.substr( a, b - a + 1 );
}

static std::string spline_lower( std::string text )
{
	for ( char& c : text ) c = (char)tolower( (unsigned char)c );
	return text;
}

// lower case without spaces, _ and -, for find:
static std::string spline_squash( const std::string& text )
{
	std::string out;
	for ( char c : text ) if ( c != ' ' && c != '_' && c != '-' ) out += (char)tolower( (unsigned char)c );
	return out;
}

static std::vector<std::string> spline_split( const std::string& text, char separator )
{
	std::vector<std::string> out;
	size_t pos = 0;
	while ( pos <= text.size() )
	{
		size_t end = text.find( separator, pos );
		if ( end == std::string::npos ) end = text.size();
		const std::string item = spline_trim( text.substr( pos, end - pos ) );
		if ( !item.empty() ) out.push_back( item );
		pos = end + 1;
	}
	return out;
}

// the material types the paint palette sets for footsteps (M-TerrainNew's Texture Material Type)
static int spline_kindcode( const std::string& name )
{
	if ( name == "grass" ) return 0;
	if ( name == "stone" ) return 1;
	if ( name == "metal" ) return 2;
	if ( name == "wood" ) return 3;
	if ( name == "snow" ) return 6;
	if ( name == "generic" || name == "tarmac" ) return 10;
	if ( name == "dirt" ) return 11;
	if ( name == "sand" ) return 13;
	return -2;
}

static const char* spline_kindname( int code )
{
	switch ( code )
	{
		case 0: return "grass";
		case 1: return "stone";
		case 2: return "metal";
		case 3: return "wood";
		case 6: return "snow";
		case 11: return "dirt";
		case 13: return "sand";
	}
	return nullptr; // generic (the default of a custom texture), or not known
}

static void spline_parsepresets( const char* pText, bool bUser, std::vector<sSplinePreset>& out )
{
	const float M = SPLINE_UNITS_PER_M;
	const std::string text( pText );
	int preset = -1;
	bool bLayer = false;
	size_t pos = 0;
	while ( pos < text.size() )
	{
		size_t end = text.find( '\n', pos );
		if ( end == std::string::npos ) end = text.size();
		const std::string line = spline_trim( text.substr( pos, end - pos ) );
		pos = end + 1;
		if ( line.empty() || line[0] == ';' || line[0] == '#' ) continue;
		if ( line[0] == '[' )
		{
			preset = -1;
			bLayer = false;
			const size_t close = line.find( ']' );
			const size_t colon = line.find( ':' );
			if ( close == std::string::npos || colon == std::string::npos || colon > close ) continue;
			const std::string kind = spline_lower( spline_trim( line.substr( 1, colon - 1 ) ) );
			sSplinePreset p;
			p.name = spline_trim( line.substr( colon + 1, close - colon - 1 ) ).substr( 0, 63 );
			p.bUser = bUser;
			if ( kind == "road" ) p.kind = SPLINE_KIND_ROAD;
			else if ( kind == "river" ) p.kind = SPLINE_KIND_RIVER;
			else continue;
			if ( p.name.empty() ) continue;
			out.push_back( p );
			preset = (int)out.size() - 1;
			continue;
		}
		if ( preset < 0 ) continue;
		sSplinePreset& p = out[ preset ];
		const size_t eq = line.find( '=' );
		if ( eq == std::string::npos ) continue;
		const std::string key = spline_lower( spline_trim( line.substr( 0, eq ) ) );
		const std::string value = spline_trim( line.substr( eq + 1 ) );
		const float f = (float)atof( value.c_str() );
		const int i = atoi( value.c_str() );
		if ( key == "layer" )
		{
			sSplineLayer layer;
			layer.keepApart = 0.0f; // the struct's default is a road lamp's
			p.layers.push_back( layer );
			p.entities.push_back( "" );
			bLayer = true;
			continue;
		}
		if ( bLayer )
		{
			sSplineLayer& l = p.layers.back();
			const std::string v = spline_lower( value );
			if ( key == "entity" ) p.entities.back() = value;
			else if ( key == "name" ) strcpy_s( l.name, 64, value.substr( 0, 63 ).c_str() );
			else if ( key == "spacing" ) l.spacing = f * M;
			else if ( key == "start" ) l.start = f * M;
			else if ( key == "side" ) l.side = v == "left" ? SPLINE_SIDE_LEFT : v == "right" ? SPLINE_SIDE_RIGHT : v == "alternate" ? SPLINE_SIDE_ALTERNATE : (v == "centre" || v == "center") ? SPLINE_SIDE_CENTRE : SPLINE_SIDE_BOTH;
			else if ( key == "offset" ) l.offset = f * M;
			else if ( key == "facing" ) l.facing = v == "along" ? SPLINE_FACE_ALONG : v == "random" ? SPLINE_FACE_RANDOM : SPLINE_FACE_MIRRORED;
			else if ( key == "turn" ) l.turn = f;
			else if ( key == "height" ) l.height = f * M;
			else if ( key == "scale" )
			{
				float a = 100.0f, b = 100.0f;
				const int n = sscanf_s( value.c_str(), "%f , %f", &a, &b );
				l.scaleMin = a;
				l.scaleMax = n >= 2 ? b : a;
			}
			else if ( key == "jitteralong" ) l.jitter = f * M;
			else if ( key == "jitteracross" ) l.jitterAcross = f * M;
			else if ( key == "keepapart" ) l.keepApart = f * M;
			else if ( key == "followslope" ) l.followSlope = i != 0;
			else if ( key == "layflat" ) l.layFlat = std::min( 2, std::max( 0, i ) );
			else if ( key == "onlyinrapids" ) l.minTurbulence = f;
			else if ( key == "on" ) l.enabled = i != 0;
			continue;
		}
		if ( key == "smoothing" ) { p.road.smoothing = f * M; p.river.smoothing = f * M; }
		else if ( key == "grassmargin" ) { p.road.grassMargin = f * M; p.river.grassMargin = f * M; }
		else if ( key == "treemargin" ) { p.road.treeMargin = f * M; p.river.treeMargin = f * M; }
		else if ( key == "width" ) p.road.width = f * M;
		else if ( key == "shoulder" ) p.road.shoulder = f * M;
		else if ( key == "maxgrade" ) p.road.maxGrade = f;
		else if ( key == "crown" ) p.road.crown = f * M;
		else if ( key == "centreline" || key == "centerline" )
		{
			const std::string v = spline_lower( value );
			p.road.markCentre = v == "dashed" ? SPLINE_MARK_DASHED : v == "solid" ? SPLINE_MARK_SOLID : v == "double" ? SPLINE_MARK_DOUBLE : v == "soliddashed" ? SPLINE_MARK_SOLIDDASHED : SPLINE_MARK_NONE;
		}
		else if ( key == "centrecolour" || key == "centercolor" ) p.road.markCentreYellow = spline_lower( value ) == "yellow";
		else if ( key == "lanes" ) p.road.markLanes = std::min( 4, std::max( 1, i ) );
		else if ( key == "edgelines" ) p.road.markEdges = i != 0;
		else if ( key == "edgecolour" || key == "edgecolor" ) p.road.markEdgeYellow = spline_lower( value ) == "yellow";
		else if ( key == "edgeinset" ) p.road.markEdgeInset = f * M;
		else if ( key == "linewidth" ) p.road.markLineWidth = f * M;
		else if ( key == "edgewidth" ) p.road.markEdgeWidth = f * M;
		else if ( key == "dash" ) p.road.markDash = f * M;
		else if ( key == "gap" ) p.road.markGap = f * M;
		else if ( key == "solidonbends" ) p.road.markBendRadius = f * M;
		else if ( key == "wear" ) p.road.markWear = f;
		else if ( key == "texture" || key == "bedtexture" ) p.textures[0] = value;
		else if ( key == "edgetexture" || key == "banktexture" ) p.textures[1] = value;
		else if ( key == "bedwidth" ) p.river.bedWidth = f * M;
		else if ( key == "depth" ) p.river.depth = f * M;
		else if ( key == "banks" ) p.river.banks = f * M;
		else if ( key == "downhill" ) p.river.downhill = i != 0;
		else if ( key == "waterdepth" ) p.river.waterDepth = f * M;
		else if ( key == "wadedepth" ) p.river.wadeDepth = f * M;
		else if ( key == "rapids" ) p.river.rapids = f;
		else if ( key == "steepflow" ) p.river.steepFlow = f;
		else if ( key == "bankfoam" ) p.river.foam = f;
		else if ( key == "raisebanks" ) p.river.raiseBanks = i != 0;
		else if ( key == "calmend" ) p.river.calmEnd = i != 0;
		else if ( key == "mainlook" ) p.river.mainLook = i != 0;
		else if ( key == "colour" || key == "color" ) sscanf_s( value.c_str(), "%f , %f , %f", &p.river.colour[0], &p.river.colour[1], &p.river.colour[2] );
		else if ( key == "clarity" ) p.river.clarity = f;
		else if ( key == "seedepth" ) p.river.seeDepth = f * M;
		else if ( key == "flow" ) p.river.flow = f;
		else if ( key == "waves" ) p.river.waves = f;
		else if ( key == "ripples" ) p.river.ripples = f;
	}
}

// the built-in presets, and the user's (in place of a built-in one of the same name)
static void spline_loadpresets( void )
{
	g_bSplinePresetsLoaded = true;
	g_SplineBuiltInPresets.clear();
	g_SplineUserPresets.clear();
	spline_parsepresets( g_pSplineBuiltInPresets, false, g_SplineBuiltInPresets );
	char pPath[ MAX_PATH ] = SPLINE_PRESET_FILE;
	GG_GetRealPath( pPath, 0 );
	FILE* fp = nullptr;
	if ( fopen_s( &fp, pPath, "rb" ) == 0 && fp )
	{
		std::string text;
		char buffer[ 4096 ];
		size_t got = 0;
		while ( (got = fread( buffer, 1, sizeof(buffer), fp )) > 0 ) text.append( buffer, got );
		fclose( fp );
		spline_parsepresets( text.c_str(), true, g_SplineUserPresets );
	}
	g_SplinePresets = g_SplineBuiltInPresets;
	for ( const sSplinePreset& user : g_SplineUserPresets )
	{
		bool bReplaced = false;
		for ( sSplinePreset& p : g_SplinePresets )
		{
			if ( p.kind != user.kind || _stricmp( p.name.c_str(), user.name.c_str() ) != 0 ) continue;
			p = user;
			bReplaced = true;
			break;
		}
		if ( !bReplaced ) g_SplinePresets.push_back( user );
	}
}

// the material slot a palette entry paints (the palette's order can differ from the slots')
static int spline_paletteslot( int iL )
{
	return sTerrainTexturesID[ 0 ] > 0 ? sTerrainSelectionID[ iL ] : iL;
}

// the stock texture (terraintextures\matN) a palette entry shows, or the one a custom texture folder's entry says it
// copies with a "matN" word in its path (15_mat15_Canyon Gravel Path\Color.dds); -1 another
static int spline_entrymat( int iL )
{
	const std::string path = spline_lower( t.visuals.sTerrainTextures[ iL ].Get() );
	const char* pMat = "terraintextures\\mat";
	const size_t at = path.find( pMat );
	if ( at != std::string::npos ) return atoi( path.c_str() + at + strlen( pMat ) );
	for ( size_t k = path.find( "mat" ); k != std::string::npos; k = path.find( "mat", k + 1 ) )
	{
		const bool bWordStart = k == 0 || path[ k - 1 ] == '\\' || path[ k - 1 ] == '/' || path[ k - 1 ] == '_' || path[ k - 1 ] == ' ' || path[ k - 1 ] == '-';
		if ( !bWordStart || k + 3 >= path.size() || !isdigit( (unsigned char)path[ k + 3 ] ) ) continue;
		size_t e = k + 3;
		while ( e < path.size() && isdigit( (unsigned char)path[ e ] ) ) e++;
		if ( e < path.size() && isalpha( (unsigned char)path[ e ] ) ) continue;
		return atoi( path.c_str() + k + 3 );
	}
	return -1;
}

// a palette entry's material type: a stock texture's from terraintextures\matsounds.txt, a custom one's as set in the
// palette; -1 not known
static int spline_entrykind( int iL )
{
	extern int g_iCustomTerrainMatSounds[32];
	if ( t.visuals.customTexturesFolder.Len() > 0 ) return g_iCustomTerrainMatSounds[ iL ];
	static int kinds[ 100 ];
	static bool bRead = false;
	if ( !bRead )
	{
		bRead = true;
		for ( int& k : kinds ) k = -1;
		char pPath[ MAX_PATH ] = "terraintextures\\matsounds.txt";
		GG_GetRealPath( pPath, 0 );
		FILE* fp = nullptr;
		if ( fopen_s( &fp, pPath, "r" ) == 0 && fp )
		{
			char line[ 128 ];
			int mat = 0, kind = 0;
			while ( fgets( line, 128, fp ) ) if ( sscanf_s( line, "mat%d=%d", &mat, &kind ) == 2 && mat >= 0 && mat < 100 ) kinds[ mat ] = kind;
			fclose( fp );
		}
	}
	const int mat = spline_entrymat( iL );
	return mat >= 0 && mat < 100 ? kinds[ mat ] : -1;
}

// texture candidates to a material slot + 1 (0 paints nothing): the first the palette has
static int spline_presettexture( const std::string& candidates )
{
	for ( const std::string& candidate : spline_split( candidates, ',' ) )
	{
		const std::string c = spline_lower( candidate );
		if ( c == "none" ) return 0;
		bool bMat = c.size() > 3 && c.compare( 0, 3, "mat" ) == 0;
		for ( size_t k = 3; bMat && k < c.size(); k++ ) if ( !isdigit( (unsigned char)c[k] ) ) bMat = false;
		for ( int iL = 0; iL < 32; iL++ )
		{
			if ( t.visuals.sTerrainTextures[ iL ].Len() == 0 ) continue;
			bool bMatch = false;
			if ( c.compare( 0, 5, "kind:" ) == 0 ) bMatch = spline_entrykind( iL ) == spline_kindcode( c.substr( 5 ) );
			else if ( bMat ) bMatch = spline_entrymat( iL ) == atoi( c.c_str() + 3 );
			else bMatch = spline_lower( t.visuals.sTerrainTexturesName[ iL ].Get() ).find( c ) != std::string::npos || spline_lower( t.visuals.sTerrainTextures[ iL ].Get() ).find( c ) != std::string::npos;
			if ( bMatch ) return spline_paletteslot( iL ) + 1;
		}
	}
	return 0;
}

// a material slot as texture candidates: its stock texture, its name if the user gave it one, its material type
static std::string spline_texturecandidates( int material )
{
	if ( material <= 0 ) return "none";
	for ( int iL = 0; iL < 32; iL++ )
	{
		if ( t.visuals.sTerrainTextures[ iL ].Len() == 0 || spline_paletteslot( iL ) != material - 1 ) continue;
		std::vector<std::string> out;
		const int mat = spline_entrymat( iL );
		if ( mat > 0 ) out.push_back( "mat" + std::to_string( mat ) );
		std::string name = t.visuals.sTerrainTexturesName[ iL ].Get();
		name.erase( std::remove( name.begin(), name.end(), ',' ), name.end() );
		bool bDefaultName = name.compare( 0, 9, "Material " ) == 0;
		for ( size_t k = 9; bDefaultName && k < name.size(); k++ ) if ( !isdigit( (unsigned char)name[k] ) ) bDefaultName = false;
		if ( !name.empty() && !bDefaultName ) out.push_back( name );
		const char* pKind = spline_kindname( spline_entrykind( iL ) );
		if ( pKind ) out.push_back( std::string( "kind:" ) + pKind );
		std::string text;
		for ( const std::string& item : out ) text += (text.empty() ? "" : ", ") + item;
		return text.empty() ? "none" : text;
	}
	return "none";
}

// every entity file in the entity banks (relative to the bank), the user's own first
static void spline_scanentities( const std::string& root, const std::string& rel, std::vector<std::string>& out )
{
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA( (root + "\\" + rel + "*").c_str(), &fd );
	if ( h == INVALID_HANDLE_VALUE ) return;
	do
	{
		if ( fd.cFileName[0] == '.' ) continue;
		if ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			if ( _stricmp( fd.cFileName, "_markers" ) != 0 ) spline_scanentities( root, rel + fd.cFileName + "\\", out );
			continue;
		}
		const size_t len = strlen( fd.cFileName );
		if ( len > 4 && _stricmp( fd.cFileName + len - 4, ".fpe" ) == 0 ) out.push_back( rel + fd.cFileName );
	}
	while ( FindNextFileA( h, &fd ) );
	FindClose( h );
}

static void spline_entitylist( std::vector<std::string>& out )
{
	out.clear();
	char pInstall[ MAX_PATH ];
	GetCurrentDirectoryA( MAX_PATH, pInstall );
	strcat_s( pInstall, MAX_PATH, "\\entitybank" );
	char pWrite[ MAX_PATH ] = "entitybank";
	GG_GetRealPath( pWrite, 0 );
	std::vector<std::string> mine, theirs;
	if ( _stricmp( pWrite, pInstall ) != 0 ) spline_scanentities( pWrite, "", mine );
	spline_scanentities( pInstall, "", theirs );
	// the user's own folder first
	for ( int pass = 0; pass < 2; pass++ )
	{
		for ( const std::string& file : mine ) if ( (_strnicmp( file.c_str(), "user\\", 5 ) == 0) == (pass == 0) ) out.push_back( file );
	}
	for ( const std::string& file : theirs ) if ( std::find( out.begin(), out.end(), file ) == out.end() ) out.push_back( file );
}

// entity candidates to an entity (relative to the bank), "" none installed
static std::string spline_presetentity( const std::string& candidates, std::vector<std::string>& files, bool* pScanned )
{
	for ( const std::string& candidate : spline_split( candidates, '|' ) )
	{
		if ( _strnicmp( candidate.c_str(), "find:", 5 ) == 0 )
		{
			const std::string words = spline_squash( candidate.substr( 5 ) );
			if ( words.empty() ) continue;
			if ( !*pScanned ) { spline_entitylist( files ); *pScanned = true; }
			for ( const std::string& file : files )
			{
				const size_t slash = file.find_last_of( '\\' );
				std::string name = slash == std::string::npos ? file : file.substr( slash + 1 );
				if ( name.size() > 4 ) name.resize( name.size() - 4 );
				if ( spline_squash( name ).find( words ) != std::string::npos ) return file;
			}
			continue;
		}
		char pPath[ MAX_PATH ];
		sprintf_s( pPath, MAX_PATH, "entitybank\\%s", candidate.c_str() );
		GG_GetRealPath( pPath, 0 );
		if ( FileExist( pPath ) ) return candidate;
	}
	return "";
}

// what a preset sets: Edited shows once these differ from when it was set
// (roadBytes, riverBytes and layerBytes: the settings as an older version had them, the fields added since left out)
static uint64_t spline_presetsignature( const sSpline& s, size_t riverBytes = sizeof(sSplineRiver), size_t layerBytes = sizeof(sSplineLayer), size_t roadBytes = sizeof(sSplineRoad) )
{
	uint64_t h = 0xcbf29ce484222325ULL;
	h = spline_hashmix( h, &s.kind, sizeof(s.kind) );
	sSplineRoad r = s.road;
	r.autoApply = 0;
	sSplineRiver v = s.river;
	v.autoApply = 0;
	if ( s.kind == SPLINE_KIND_ROAD ) h = spline_hashmix( h, &r, roadBytes );
	if ( s.kind == SPLINE_KIND_RIVER ) h = spline_hashmix( h, &v, riverBytes );
	for ( const sSplineLayer& layer : s.layers ) if ( !layer.frozen ) h = spline_hashmix( h, &layer, layerBytes );
	return h ? h : 1;
}

// sets a spline from a preset: its settings and textures, and its layers in place of those it had (a frozen layer stays,
// with its entities); Update as I Edit stays as it was
static void spline_applypreset( sSpline& s, const sSplinePreset& p )
{
	spline_unplace( s );
	s.kind = p.kind;
	const int textures[2] = { spline_presettexture( p.textures[0] ), spline_presettexture( p.textures[1] ) };
	if ( p.kind == SPLINE_KIND_ROAD )
	{
		const int autoApply = s.road.autoApply;
		s.road = p.road;
		s.road.autoApply = autoApply;
		s.road.material = textures[0];
		s.road.edgeMaterial = textures[1];
	}
	else
	{
		const int autoApply = s.river.autoApply;
		s.river = p.river;
		s.river.autoApply = autoApply;
		s.river.bedMaterial = textures[0];
		s.river.bankMaterial = textures[1];
	}
	std::vector<sSplineLayer> layers;
	std::vector<int> moved( s.layers.size(), -1 );
	for ( int li = 0; li < (int)s.layers.size(); li++ )
	{
		if ( !s.layers[ li ].frozen ) continue;
		moved[ li ] = (int)layers.size();
		layers.push_back( s.layers[ li ] );
	}
	for ( sSplinePlaced& placed : s.placed ) if ( placed.layer >= 0 && placed.layer < (int)moved.size() ) placed.layer = moved[ placed.layer ];
	std::vector<std::string> files;
	bool bScanned = false;
	for ( size_t li = 0; li < p.layers.size(); li++ )
	{
		sSplineLayer layer = p.layers[ li ];
		strcpy_s( layer.entity, 260, spline_presetentity( p.entities[ li ], files, &bScanned ).c_str() );
		layers.push_back( layer );
	}
	s.layers.swap( layers );
	strcpy_s( s.preset, 64, p.name.c_str() );
	s.presetSignature = spline_presetsignature( s );
}

static std::string spline_number( float value )
{
	char text[ 32 ];
	sprintf_s( text, 32, "%.3f", value );
	char* pEnd = text + strlen( text ) - 1;
	while ( pEnd > text && *pEnd == '0' ) *pEnd-- = 0;
	if ( *pEnd == '.' ) *pEnd = 0;
	return text;
}

static void spline_writepreset( std::string& out, const sSplinePreset& p )
{
	const float M = SPLINE_UNITS_PER_M;
	auto line = [&out]( const char* pKey, const std::string& value ) { out += pKey; out += " = "; out += value; out += "\n"; };
	auto metres = [&]( const char* pKey, float units ) { line( pKey, spline_number( units / M ) ); };
	auto number = [&]( const char* pKey, float value ) { line( pKey, spline_number( value ) ); };
	out += std::string( "[" ) + (p.kind == SPLINE_KIND_RIVER ? "river" : "road") + ": " + p.name + "]\n";
	if ( p.kind == SPLINE_KIND_ROAD )
	{
		const sSplineRoad& r = p.road;
		metres( "width", r.width );
		metres( "shoulder", r.shoulder );
		metres( "smoothing", r.smoothing );
		number( "maxgrade", r.maxGrade );
		metres( "crown", r.crown );
		metres( "grassmargin", r.grassMargin );
		metres( "treemargin", r.treeMargin );
		line( "texture", p.textures[0] );
		line( "edgetexture", p.textures[1] );
		const char* centres[] = { "none", "dashed", "solid", "double", "soliddashed" };
		line( "centreline", centres[ std::min( 4, std::max( 0, r.markCentre ) ) ] );
		if ( r.markCentre != SPLINE_MARK_NONE ) line( "centrecolour", r.markCentreYellow ? "yellow" : "white" );
		number( "lanes", (float)r.markLanes );
		number( "edgelines", (float)r.markEdges );
		if ( r.markEdges )
		{
			line( "edgecolour", r.markEdgeYellow ? "yellow" : "white" );
			metres( "edgeinset", r.markEdgeInset );
			metres( "edgewidth", r.markEdgeWidth );
		}
		metres( "linewidth", r.markLineWidth );
		metres( "dash", r.markDash );
		metres( "gap", r.markGap );
		metres( "solidonbends", r.markBendRadius );
		number( "wear", r.markWear );
	}
	else
	{
		const sSplineRiver& v = p.river;
		metres( "bedwidth", v.bedWidth );
		metres( "depth", v.depth );
		metres( "banks", v.banks );
		metres( "smoothing", v.smoothing );
		metres( "grassmargin", v.grassMargin );
		metres( "treemargin", v.treeMargin );
		line( "bedtexture", p.textures[0] );
		line( "banktexture", p.textures[1] );
		number( "downhill", (float)v.downhill );
		metres( "waterdepth", v.waterDepth );
		metres( "wadedepth", v.wadeDepth );
		number( "rapids", v.rapids );
		number( "steepflow", v.steepFlow );
		number( "bankfoam", v.foam );
		number( "raisebanks", (float)v.raiseBanks );
		number( "calmend", (float)v.calmEnd );
		number( "mainlook", (float)v.mainLook );
		if ( !v.mainLook )
		{
			line( "colour", spline_number( v.colour[0] ) + ", " + spline_number( v.colour[1] ) + ", " + spline_number( v.colour[2] ) );
			number( "clarity", v.clarity );
			metres( "seedepth", v.seeDepth );
			number( "flow", v.flow );
			number( "waves", v.waves );
			number( "ripples", v.ripples );
		}
	}
	const char* sides[] = { "both", "left", "right", "alternate", "centre" };
	const char* faces[] = { "mirrored", "along", "random" };
	for ( size_t li = 0; li < p.layers.size(); li++ )
	{
		const sSplineLayer& l = p.layers[ li ];
		const char* pName = strrchr( l.entity, '\\' );
		line( "layer", pName ? pName + 1 : (l.entity[0] ? l.entity : "Layer") );
		line( "entity", p.entities[ li ] );
		if ( l.name[0] ) line( "name", l.name );
		metres( "spacing", l.spacing );
		metres( "start", l.start );
		line( "side", sides[ std::min( 4, std::max( 0, l.side ) ) ] );
		metres( "offset", l.offset );
		line( "facing", faces[ std::min( 2, std::max( 0, l.facing ) ) ] );
		number( "turn", l.turn );
		metres( "height", l.height );
		line( "scale", spline_number( l.scaleMin ) + ", " + spline_number( l.scaleMax ) );
		metres( "jitteralong", l.jitter );
		metres( "jitteracross", l.jitterAcross );
		metres( "keepapart", l.keepApart );
		number( "followslope", (float)l.followSlope );
		number( "layflat", (float)l.layFlat );
		if ( p.kind == SPLINE_KIND_RIVER ) number( "onlyinrapids", l.minTurbulence );
		number( "on", (float)l.enabled );
	}
	out += "\n";
}

static bool spline_savepresetfile( void )
{
	std::string out = g_pSplinePresetFileHeader;
	out += "\n";
	for ( const sSplinePreset& p : g_SplineUserPresets ) spline_writepreset( out, p );
	char pPath[ MAX_PATH ] = SPLINE_PRESET_FILE;
	GG_GetRealPath( pPath, 1 );
	FILE* fp = nullptr;
	if ( fopen_s( &fp, pPath, "wb" ) != 0 || !fp ) return false;
	fwrite( out.data(), 1, out.size(), fp );
	fclose( fp );
	return true;
}

static int spline_findpreset( const std::vector<sSplinePreset>& presets, int kind, const char* pName )
{
	for ( int pi = 0; pi < (int)presets.size(); pi++ ) if ( presets[ pi ].kind == kind && _stricmp( presets[ pi ].name.c_str(), pName ) == 0 ) return pi;
	return -1;
}

// the spline's settings, textures and layers as the user's own preset (one of the same name replaced)
static bool spline_saveaspreset( sSpline& s, const char* pName )
{
	sSplinePreset p;
	p.name = pName;
	p.kind = s.kind;
	p.bUser = true;
	p.road = s.road;
	p.river = s.river;
	p.road.autoApply = p.river.autoApply = 1;
	if ( s.kind == SPLINE_KIND_ROAD )
	{
		p.textures[0] = spline_texturecandidates( s.road.material );
		p.textures[1] = spline_texturecandidates( s.road.edgeMaterial );
	}
	else
	{
		p.textures[0] = spline_texturecandidates( s.river.bedMaterial );
		p.textures[1] = spline_texturecandidates( s.river.bankMaterial );
	}
	for ( const sSplineLayer& layer : s.layers )
	{
		sSplineLayer l = layer;
		l.frozen = 0;
		p.layers.push_back( l );
		p.entities.push_back( layer.entity );
	}
	const int at = spline_findpreset( g_SplineUserPresets, p.kind, pName );
	if ( at >= 0 ) g_SplineUserPresets[ at ] = p;
	else g_SplineUserPresets.push_back( p );
	const bool bSaved = spline_savepresetfile();
	spline_loadpresets();
	strcpy_s( s.preset, 64, pName );
	s.presetSignature = spline_presetsignature( s );
	return bSaved;
}

// the Preset row (a road's or a river's), and Save as Preset
static void spline_rowpreset( sSpline& s )
{
	if ( !g_bSplinePresetsLoaded ) spline_loadpresets();
	char preview[ 96 ];
	if ( !s.preset[0] ) strcpy_s( preview, 96, "Choose a preset" );
	else sprintf_s( preview, 96, "%s%s", s.preset, spline_presetsignature( s ) != s.presetSignature ? " (edited)" : "" );
	spline_row( "Preset" );
	if ( ImGui::BeginCombo( "##splinepreset", preview ) )
	{
		for ( const sSplinePreset& p : g_SplinePresets )
		{
			if ( p.kind != s.kind ) continue;
			char label[ 96 ];
			sprintf_s( label, 96, "%s%s##splinepreset%s", p.name.c_str(), p.bUser ? " (yours)" : "", p.name.c_str() );
			if ( ImGui::Selectable( label, _stricmp( p.name.c_str(), s.preset ) == 0 ) )
			{
				spline_applypreset( s, p );
				spline_modified();
			}
		}
		ImGui::EndCombo();
	}
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", s.kind == SPLINE_KIND_ROAD ? "Sets the road's width, shape, textures and layers (streetlights, decals) from a preset; change any of them after.\nTextures come from this level's paint palette, entities from your entity bank (your own first)." : "Sets the river's size, water, textures and layers (rocks) from a preset; change any of them after.\nTextures come from this level's paint palette, entities from your entity bank (your own first)." );
	static char saveName[ 64 ] = "";
	ImGui::SetCursorPosX( fRowFieldX );
	if ( ImGui::StyleButton( "Save as Preset...##splinesavepreset", ImVec2( fRowRight - fRowFieldX, 0 ) ) )
	{
		strcpy_s( saveName, 64, s.preset );
		ImGui::OpenPopup( "##splinesavepresetpopup" );
	}
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Keep these settings, textures and layers as a preset of your own (editors\\splinepresets.txt in your writable Files folder)" );
	if ( ImGui::BeginPopup( "##splinesavepresetpopup" ) )
	{
		ImGui::TextUnformatted( "Preset name" );
		ImGui::SetNextItemWidth( 260.0f );
		ImGui::InputText( "##splinepresetname", saveName, 64 );
		const int mine = spline_findpreset( g_SplineUserPresets, s.kind, saveName );
		if ( mine >= 0 ) ImGui::TextUnformatted( "Replaces your preset of that name." );
		else if ( spline_findpreset( g_SplineBuiltInPresets, s.kind, saveName ) >= 0 ) ImGui::TextUnformatted( "Replaces the built-in preset of that name, for you." );
		if ( ImGui::StyleButton( "Save##splinepresetsave", ImVec2( 80.0f, 0 ) ) && spline_trim( saveName ).size() > 0 )
		{
			const std::string name = spline_trim( saveName );
			spline_saveaspreset( s, name.c_str() );
			spline_modified();
			ImGui::CloseCurrentPopup();
		}
		if ( mine >= 0 )
		{
			ImGui::SameLine();
			if ( ImGui::StyleButton( "Delete Mine##splinepresetdelete", ImVec2( 110.0f, 0 ) ) )
			{
				g_SplineUserPresets.erase( g_SplineUserPresets.begin() + mine );
				spline_savepresetfile();
				spline_loadpresets();
				ImGui::CloseCurrentPopup();
			}
			if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Delete your preset of this name (a built-in one of the name comes back)" );
		}
		ImGui::SameLine();
		if ( ImGui::StyleButton( "Cancel##splinepresetcancel", ImVec2( 80.0f, 0 ) ) ) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

//
// Undo
//

// a spline as the user set it (what an undo puts back), and how its bake stood: 0 not baked, 1 baked as it is, 2 baked
// before it last changed (Update as I Edit off)
struct sSplineDef
{
	int id = 0;
	char name[64] = "";
	int kind = SPLINE_KIND_NONE;
	int curve = SPLINE_CURVE_SMOOTH;
	int closed = 0;
	std::vector<sSplineNode> nodes;
	sSplineRoad road;
	sSplineRiver river;
	std::vector<sSplineLayer> layers;
	char preset[64] = "";
	uint64_t presetSignature = 0;
	int bakeState = 0;
};

// one undo or redo step: the splines it changed as they were (one that didn't exist then is marked so), the list's order
// and the spline selected
struct sSplineUndoState
{
	bool bExists = false;
	sSplineDef def;
};
struct sSplineUndoEvent
{
	int generation = 0;
	std::vector<int> order;
	std::vector<sSplineUndoState> states;
	int selected = 0;
};

static std::vector<sSplineDef> g_SplineUndoBase; // the splines as of the last step
static int g_iSplineUndoBaseSelected = 0;
static bool g_bSplineUndoBaseSet = false;
static int g_iSplineUndoGeneration = 1; // a level's steps do nothing in another (the editor keeps its undo list for a new level)

static int spline_bakestate( const sSpline& s )
{
	if ( s.baked.empty() && s.bakedTrees.empty() ) return 0;
	return spline_signature( s ) == s.bakedSignature ? 1 : 2;
}

static void spline_getdef( const sSpline& s, sSplineDef& def )
{
	def.id = s.id;
	memcpy( def.name, s.name, sizeof(def.name) );
	def.kind = s.kind;
	def.curve = s.curve;
	def.closed = s.closed;
	def.nodes = s.nodes;
	def.road = s.road;
	def.river = s.river;
	def.layers = s.layers;
	memcpy( def.preset, s.preset, sizeof(def.preset) );
	def.presetSignature = s.presetSignature;
	def.bakeState = spline_bakestate( s );
}

static void spline_setdef( sSpline& s, const sSplineDef& def )
{
	s.id = def.id;
	memcpy( s.name, def.name, sizeof(s.name) );
	s.kind = def.kind;
	s.curve = def.curve;
	s.closed = def.closed;
	s.nodes = def.nodes;
	s.road = def.road;
	s.river = def.river;
	s.layers = def.layers;
	memcpy( s.preset, def.preset, sizeof(s.preset) );
	s.presetSignature = def.presetSignature;
}

static bool spline_samedef( const sSpline& s, const sSplineDef& def )
{
	if ( s.kind != def.kind || s.curve != def.curve || s.closed != def.closed || memcmp( s.name, def.name, sizeof(s.name) ) != 0 ) return false;
	if ( memcmp( s.preset, def.preset, sizeof(s.preset) ) != 0 || s.presetSignature != def.presetSignature ) return false;
	if ( s.nodes.size() != def.nodes.size() || s.layers.size() != def.layers.size() ) return false;
	if ( !s.nodes.empty() && memcmp( s.nodes.data(), def.nodes.data(), s.nodes.size() * sizeof(sSplineNode) ) != 0 ) return false;
	if ( !s.layers.empty() && memcmp( s.layers.data(), def.layers.data(), s.layers.size() * sizeof(sSplineLayer) ) != 0 ) return false;
	return memcmp( &s.road, &def.road, sizeof(s.road) ) == 0 && memcmp( &s.river, &def.river, sizeof(s.river) ) == 0;
}

static int spline_indexofid( int id )
{
	for ( int si = 0; si < (int)g_Splines.size(); si++ ) if ( g_Splines[ si ].id == id ) return si;
	return -1;
}

static int spline_selectedid( void )
{
	return g_iSplineSelected >= 0 && g_iSplineSelected < (int)g_Splines.size() ? g_Splines[ g_iSplineSelected ].id : 0;
}

static void spline_undosetbase( void )
{
	g_SplineUndoBase.resize( g_Splines.size() );
	for ( size_t si = 0; si < g_Splines.size(); si++ ) spline_getdef( g_Splines[ si ], g_SplineUndoBase[ si ] );
	g_iSplineUndoBaseSelected = spline_selectedid();
	g_bSplineUndoBaseSet = true;
}

// once an edit is finished (nothing dragged or being edited), what changed since the last step becomes the next step on
// the editor's undo list: the splines it changed as they were before. Every edit in the panel and the view is caught so
static void spline_undocommit( void )
{
	if ( !g_bSplineUndoBaseSet ) { spline_undosetbase(); return; }
	if ( iDragNode >= 0 || ImGui::IsAnyItemActive() ) return;
	std::unordered_map<int, int> baseIndex;
	for ( int bi = 0; bi < (int)g_SplineUndoBase.size(); bi++ ) baseIndex[ g_SplineUndoBase[ bi ].id ] = bi;
	std::vector<sSplineUndoState> states;
	bool bOrder = g_Splines.size() != g_SplineUndoBase.size();
	std::unordered_set<int> liveIds;
	for ( int si = 0; si < (int)g_Splines.size(); si++ )
	{
		const sSpline& s = g_Splines[ si ];
		liveIds.insert( s.id );
		auto it = baseIndex.find( s.id );
		if ( it == baseIndex.end() )
		{
			sSplineUndoState state;
			state.def.id = s.id;
			states.push_back( state );
			continue;
		}
		sSplineDef& base = g_SplineUndoBase[ it->second ];
		if ( it->second != si ) bOrder = true;
		if ( spline_samedef( s, base ) )
		{
			base.bakeState = spline_bakestate( s ); // an Apply, or a bake that had to wait, isn't a step of its own
			continue;
		}
		sSplineUndoState state;
		state.bExists = true;
		state.def = base;
		states.push_back( state );
	}
	for ( const sSplineDef& base : g_SplineUndoBase )
	{
		if ( liveIds.count( base.id ) ) continue;
		sSplineUndoState state;
		state.bExists = true;
		state.def = base;
		states.push_back( state );
	}
	if ( states.empty() && !bOrder )
	{
		g_iSplineUndoBaseSelected = spline_selectedid();
		return;
	}
	sSplineUndoEvent* pEvent = new sSplineUndoEvent;
	pEvent->generation = g_iSplineUndoGeneration;
	for ( const sSplineDef& base : g_SplineUndoBase ) pEvent->order.push_back( base.id );
	pEvent->states.swap( states );
	pEvent->selected = g_iSplineUndoBaseSelected;
	undosys_clearredostack();
	undosys_addevent( eUndoSys_Spline, eUndoSys_Spline_Change, pEvent );
	spline_undosetbase();
}

// performs an undo or redo step from the editor's undo system: its opposite (these splines as they are now) goes on the
// other list first, then the splines it holds are put back, deleted or made again, the list is ordered as it was, and each
// one's bake is made to stand as it did. The layers place again, and a river's water is built again, by themselves
void spline_performundoredo( void* pEventData )
{
	sSplineUndoEvent* pEvent = (sSplineUndoEvent*)pEventData;
	if ( !pEvent ) return;
	if ( pEvent->generation != g_iSplineUndoGeneration )
	{
		delete pEvent;
		return;
	}
	sSplineUndoEvent* pOpposite = new sSplineUndoEvent;
	pOpposite->generation = g_iSplineUndoGeneration;
	for ( const sSpline& s : g_Splines ) pOpposite->order.push_back( s.id );
	pOpposite->selected = spline_selectedid();
	for ( const sSplineUndoState& state : pEvent->states )
	{
		sSplineUndoState now;
		const int si = spline_indexofid( state.def.id );
		now.bExists = si >= 0;
		if ( si >= 0 ) spline_getdef( g_Splines[ si ], now.def );
		else now.def.id = state.def.id;
		pOpposite->states.push_back( now );
	}
	undosys_addevent( eUndoSys_Spline, eUndoSys_Spline_Change, pOpposite );

	// nothing half done carries over
	iDragSpline = iDragNode = -1;
	iDragHandle = 0;
	bDrawing = false;
	bConnectMode = false;

	for ( const sSplineUndoState& state : pEvent->states )
	{
		if ( state.bExists ) continue;
		const int si = spline_indexofid( state.def.id );
		if ( si >= 0 ) spline_deletespline( si );
	}
	for ( const sSplineUndoState& state : pEvent->states )
	{
		if ( !state.bExists ) continue;
		const int si = spline_indexofid( state.def.id );
		if ( si >= 0 )
		{
			spline_setdef( g_Splines[ si ], state.def );
			continue;
		}
		sSpline s;
		spline_setdef( s, state.def );
		g_Splines.push_back( s );
		if ( state.def.id >= g_iSplineNextID ) g_iSplineNextID = state.def.id + 1;
	}
	{
		std::unordered_map<int, int> index;
		for ( int si = 0; si < (int)g_Splines.size(); si++ ) index[ g_Splines[ si ].id ] = si;
		std::vector<char> used( g_Splines.size(), 0 );
		std::vector<sSpline> ordered;
		ordered.reserve( g_Splines.size() );
		for ( int id : pEvent->order )
		{
			auto it = index.find( id );
			if ( it == index.end() || used[ it->second ] ) continue;
			used[ it->second ] = 1;
			ordered.push_back( std::move( g_Splines[ it->second ] ) );
		}
		for ( size_t si = 0; si < g_Splines.size(); si++ ) if ( !used[ si ] ) ordered.push_back( std::move( g_Splines[ si ] ) );
		g_Splines.swap( ordered );
		// a baked spline whose place in the list changed is baked again (the order decides which road a junction pins)
		for ( int si = 0; si < (int)g_Splines.size(); si++ )
		{
			const sSpline& moved = g_Splines[ si ];
			auto it = index.find( moved.id );
			if ( it != index.end() && it->second != si && (!moved.baked.empty() || !moved.bakedTrees.empty()) ) g_SplineForceBake.insert( moved.id );
		}
	}
	// the bake as it stood: taken away if it wasn't baked, baked again if it was baked as it is (or is made again), left as
	// it is if its bake was already behind it
	for ( const sSplineUndoState& state : pEvent->states )
	{
		if ( !state.bExists ) continue;
		const int si = spline_indexofid( state.def.id );
		if ( si < 0 ) continue;
		sSpline& s = g_Splines[ si ];
		const bool bBaked = !s.baked.empty() || !s.bakedTrees.empty();
		if ( state.def.bakeState == 0 )
		{
			if ( bBaked ) spline_unbake( si );
			s.bakedSignature = 0;
			g_SplineForceBake.erase( s.id );
		}
		else if ( state.def.bakeState == 1 || !bBaked )
		{
			if ( !bBaked || spline_signature( s ) != s.bakedSignature ) g_SplineForceBake.insert( s.id );
		}
	}
	g_iSplineSelected = pEvent->selected ? spline_indexofid( pEvent->selected ) : -1;
	g_iSplineNodeSelected = -1;
	g_iSplineSegSelected = -1;
	g.projectmodified = 1;
	delete pEvent;
	spline_undosetbase();
}

// the undo system frees a step it no longer holds
void spline_undodelete( void* pEventData )
{
	delete (sSplineUndoEvent*)pEventData;
}

// the editor, every frame: with the Roads and Rivers panel not drawn, a spline's undo or redo still bakes and places
void spline_editorupdate( void )
{
	if ( ImGui::GetFrameCount() - g_iSplinePanelFrame <= 2 ) return;
	spline_bakechanged();
	spline_placechanged();
}

// Update as I Edit, Apply and Remove for a road or a river, and what its bake holds
static void spline_rowbake( sSpline& s, int* pAutoApply, const char* pWhat, float w )
{
	bool bAuto = *pAutoApply != 0;
	ImGui::SetCursorPosX( fRowLabelX );
	if ( ImGui::Checkbox( "Update as I Edit##splineautoapply", &bAuto ) ) *pAutoApply = bAuto ? 1 : 0;
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Bake it into the terrain again whenever it changes" );
	ImGui::SetCursorPosX( fRowLabelX );
	if ( ImGui::StyleButton( "Apply##splineapply", ImVec2( w * 0.45f, 0 ) ) ) iApplySpline = g_iSplineSelected;
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Bake it into the terrain now" );
	ImGui::SameLine();
	char remove[ 64 ];
	sprintf_s( remove, 64, "Remove %s##splineremove", pWhat );
	if ( ImGui::StyleButton( remove, ImVec2( w * 0.45f, 0 ) ) )
	{
		spline_unbake( g_iSplineSelected );
		s.kind = SPLINE_KIND_NONE;
		s.bakedSignature = spline_signature( s );
		spline_modified();
	}
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Take it out of the terrain and keep the spline as a line" );
	if ( s.baked.empty() ) ImGui::TextWrapped( "%s", *pAutoApply ? "Not baked yet." : "Not baked: press Apply." );
	else ImGui::Text( "Baked: %d texels, %d trees hidden%s", (int)s.baked.size(), (int)s.bakedTrees.size(), spline_signature( s ) != s.bakedSignature ? " (changed)" : "" );
}

void spline_imgui_panel( float w )
{
	// edit mode ends when another terrain tool is chosen, or when the panel was not shown
	const int frame = ImGui::GetFrameCount();
	if ( frame - g_iSplinePanelFrame > 2 || bForceKey2 ) g_bSplineEditMode = false;
	if ( frame - g_iSplinePanelFrame > 2 || !bExternal_Entities_Window ) g_bLibraryPickEntity = false;
	g_iSplinePanelFrame = frame;
	if ( g_iSplineSelected >= (int)g_Splines.size() ) { g_iSplineSelected = -1; g_iSplineNodeSelected = -1; }
	if ( g_iSplineSelected >= 0 && g_iSplineNodeSelected >= (int)g_Splines[ g_iSplineSelected ].nodes.size() ) g_iSplineNodeSelected = -1;
	if ( g_iSplineSelected < 0 || g_iSplineSegSelected >= spline_segments( g_Splines[ g_iSplineSelected ] ) ) g_iSplineSegSelected = -1;

	// the section open stops the terrain's brush (its settings are edited over the 3D view), and choosing a terrain tool
	// closes it, so that tool's brush works again
	if ( bForceKey2 ) ImGui::SetNextItemOpen( false, ImGuiCond_Always );
	const bool bSectionOpen = ImGui::StyleCollapsingHeader( "Roads and Rivers", 0 );
	if ( !bSectionOpen ) g_bSplineEditMode = false;
	if ( bSectionOpen )
	{
		ImGui::Indent( 10 );
		// Edit Splines once there is a spline to edit (New Spline turns it on)
		if ( g_Splines.empty() ) g_bSplineEditMode = false;
		else if ( ImGui::Checkbox( "Edit Splines##splineeditmode", &g_bSplineEditMode ) )
		{
			bConnectMode = false;
		}
		if ( !g_Splines.empty() && ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Click the terrain to add nodes and drag them. Shift+click the curve inserts a node, Ctrl+click deletes one.\nA dragged node snaps to another spline's node or curve and joins it (a T junction or a crossing); Alt+drag pulls it out again.\nAn end dropped on the spline's other end closes it. Alt+drag a Bezier handle to break the pair." );

		if ( g_bSplineEditMode && bDrawing ) ImGui::TextWrapped( "%s", "Drawing: click the terrain to add nodes, or another spline's node or curve to join it. Esc to stop." );
		if ( ImGui::StyleButton( "New Spline##splinenew", ImVec2( w * 0.45f, 0 ) ) )
		{
			g_iSplineSelected = spline_new();
			g_iSplineNodeSelected = -1;
			g_bSplineEditMode = true;
			bDrawing = true;
			spline_modified();
		}
		ImGui::SameLine();
		if ( ImGui::StyleButton( "Delete Spline##splinedelete", ImVec2( w * 0.45f, 0 ) ) && g_iSplineSelected >= 0 )
		{
			spline_deletespline( g_iSplineSelected );
		}

		if ( !g_Splines.empty() )
		{
			ImGui::BeginChild( "##splinelist", ImVec2( 0, ImGui::GetFontSize() * 6.0f ), true );
			for ( int si = 0; si < (int)g_Splines.size(); si++ )
			{
				char label[ 96 ];
				sprintf_s( label, 96, "%s##spline%d", g_Splines[ si ].name, g_Splines[ si ].id );
				if ( ImGui::Selectable( label, si == g_iSplineSelected ) )
				{
					g_iSplineSelected = si;
					g_iSplineNodeSelected = -1;
					g_iSplineSegSelected = -1;
					bDrawing = false;
				}
			}
			ImGui::EndChild();

			// the list's order is the bake's: at a junction the later road is pinned to the earlier one's surface and stops
			// its lines at its edge, so moving a road up makes it the main road; the two swapped are baked again (with the
			// splines joined to them)
			const char* pOrderTip = "Roads earlier in the list win at a junction: a later road is pinned to the earlier one's surface there and stops its lines at its edge.\nMove the main road above the roads that join or cross it.";
			int swapWith = -1;
			if ( ImGui::StyleButton( "Move Up##splinemoveup", ImVec2( w * 0.45f, 0 ) ) && g_iSplineSelected > 0 ) swapWith = g_iSplineSelected - 1;
			if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", pOrderTip );
			ImGui::SameLine();
			if ( ImGui::StyleButton( "Move Down##splinemovedown", ImVec2( w * 0.45f, 0 ) ) && g_iSplineSelected >= 0 && g_iSplineSelected + 1 < (int)g_Splines.size() ) swapWith = g_iSplineSelected + 1;
			if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", pOrderTip );
			if ( swapWith >= 0 )
			{
				std::swap( g_Splines[ g_iSplineSelected ], g_Splines[ swapWith ] );
				for ( int si : { g_iSplineSelected, swapWith } )
				{
					const sSpline& moved = g_Splines[ si ];
					if ( !moved.baked.empty() || !moved.bakedTrees.empty() ) g_SplineForceBake.insert( moved.id );
				}
				g_iSplineSelected = swapWith;
				g_iSplineNodeSelected = -1;
				g_iSplineSegSelected = -1;
				spline_modified();
			}
		}

		if ( g_iSplineSelected >= 0 )
		{
			sSpline& s = g_Splines[ g_iSplineSelected ];
			fRowLabelX = ImGui::GetCursorPosX();
			fRowFieldX = fRowLabelX + ImGui::CalcTextSize( "Steep Flow Speed" ).x + 12.0f;
			fRowRight = ImGui::GetWindowContentRegionMax().x - 10.0f;
			if ( fRowRight < fRowFieldX + 60.0f ) fRowRight = fRowFieldX + 60.0f;

			spline_row( "Name" );
			if ( ImGui::InputText( "##splinename", s.name, 64 ) ) spline_modified();
			const char* curves[] = { "Straight", "Smooth", "Bezier" };
			int curve = s.curve;
			spline_row( "Curve" );
			if ( ImGui::Combo( "##splinecurve", &curve, curves, 3 ) && curve != s.curve )
			{
				if ( curve == SPLINE_CURVE_BEZIER ) spline_seedhandles( s, s.curve );
				s.curve = curve;
				spline_modified();
			}
			const char* kinds[] = { "Line only", "Road", "River" };
			spline_row( "Type" );
			if ( ImGui::Combo( "##splinekind", &s.kind, kinds, 3 ) )
			{
				s.preset[0] = 0;
				spline_modified();
			}
			if ( s.kind == SPLINE_KIND_ROAD || s.kind == SPLINE_KIND_RIVER ) spline_rowpreset( s );

			if ( s.kind == SPLINE_KIND_ROAD )
			{
				sSplineRoad& r = s.road;
				bool bChanged = false;
				bChanged |= spline_rowmetres( "Width", "##splineroadwidth", &r.width, 2.0f, 40.0f );
				bChanged |= spline_rowmetres( "Shoulders", "##splineroadshoulder", &r.shoulder, 0.0f, 30.0f );
				bChanged |= spline_rowmetres( "Smoothing", "##splineroadsmoothing", &r.smoothing, 0.0f, 200.0f, "%.0f m" );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The road's height is averaged over this length, so it rides over bumps" );
				spline_row( "Max Grade" );
				bChanged |= ImGui::SliderFloat( "##splineroadgrade", &r.maxGrade, 1.0f, 40.0f, "%.0f %%" );
				bChanged |= spline_rowmetres( "Crown", "##splineroadcrown", &r.crown, 0.0f, 0.5f, "%.2f m" );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "How far the centre stands above the edges, so the road sheds water" );
				spline_rowtexture( "Road Texture", "##splineroadtexture", &r.material, w );
				spline_rowtexture( "Shoulder Texture", "##splineroadedgetexture", &r.edgeMaterial, w );
				bChanged |= spline_rowmetres( "Clear Grass", "##splineroadgrass", &r.grassMargin, 0.0f, 20.0f );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Grass is cleared this far past the edge of the road" );
				bChanged |= spline_rowmetres( "Clear Trees", "##splineroadtrees", &r.treeMargin, 0.0f, 30.0f );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Trees are hidden this far past the edge of the road" );

				// the painted lines, drawn into the terrain (no Apply needed)
				const char* centres[] = { "None", "Dashed", "Solid", "Double Solid", "Solid and Dashed" };
				const char* colours[] = { "White", "Yellow" };
				spline_row( "Centre Line" );
				bChanged |= ImGui::Combo( "##splinemarkcentre", &r.markCentre, centres, 5 );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The painted line down the middle. Solid and Dashed: solid on the left as seen from the first node, dashed on the right (overtaking from the right only)" );
				if ( r.markCentre != SPLINE_MARK_NONE )
				{
					spline_row( "Centre Colour" );
					bChanged |= ImGui::Combo( "##splinemarkcentrecolour", &r.markCentreYellow, colours, 2 );
				}
				spline_row( "Lanes Each Way" );
				bChanged |= ImGui::SliderInt( "##splinemarklanes", &r.markLanes, 1, 4 );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Dashed lines between the lanes, the road's width split evenly between them" );
				bool bEdges = r.markEdges != 0;
				ImGui::SetCursorPosX( fRowFieldX );
				if ( ImGui::Checkbox( "Edge Lines##splinemarkedges", &bEdges ) ) { r.markEdges = bEdges ? 1 : 0; bChanged = true; }
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Solid lines along both edges of the road" );
				if ( r.markEdges )
				{
					spline_row( "Edge Colour" );
					bChanged |= ImGui::Combo( "##splinemarkedgecolour", &r.markEdgeYellow, colours, 2 );
					bChanged |= spline_rowmetres( "Edge Inset", "##splinemarkedgeinset", &r.markEdgeInset, 0.0f, 1.0f, "%.2f m" );
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The edge lines this far in from the edge of the road" );
					bChanged |= spline_rowmetres( "Edge Line Width", "##splinemarkedgewidth", &r.markEdgeWidth, 0.05f, 0.5f, "%.2f m" );
				}
				if ( r.markCentre != SPLINE_MARK_NONE || r.markLanes > 1 || r.markEdges )
				{
					if ( r.markCentre != SPLINE_MARK_NONE || r.markLanes > 1 ) bChanged |= spline_rowmetres( "Line Width", "##splinemarkwidth", &r.markLineWidth, 0.05f, 0.3f, "%.2f m" );
					bChanged |= spline_rowmetres( "Dash", "##splinemarkdash", &r.markDash, 0.5f, 12.0f );
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The length of a dash in a dashed line" );
					bChanged |= spline_rowmetres( "Gap", "##splinemarkgap", &r.markGap, 0.5f, 24.0f );
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The gap after each dash" );
					bChanged |= spline_rowmetres( "Solid on Bends", "##splinemarkbends", &r.markBendRadius, 0.0f, 500.0f, "%.0f m" );
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "A dashed centre line goes solid where the road bends tighter than this radius (and 15 m either side), with warning dashes before and after; 0 never" );
					spline_row( "Wear" );
					bChanged |= ImGui::SliderFloat( "##splinemarkwear", &r.markWear, 0.0f, 1.0f, "%.2f" );
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "0 fresh paint; higher wears the lines away in patches (old country roads)" );
				}
				if ( bChanged ) spline_modified();
				spline_rowbake( s, &r.autoApply, "Road", w );
				spline_rowlayers( s, w );
			}
			else if ( s.kind == SPLINE_KIND_RIVER )
			{
				sSplineRiver& v = s.river;
				bool bChanged = false;
				bChanged |= spline_rowmetres( "Bed Width", "##splineriverbed", &v.bedWidth, 1.0f, 80.0f );
				bChanged |= spline_rowmetres( "Depth", "##splineriverdepth", &v.depth, 0.5f, 30.0f );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "How far the bed lies below the averaged ground" );
				bChanged |= spline_rowmetres( "Banks", "##splineriverbanks", &v.banks, 0.0f, 50.0f );
				bChanged |= spline_rowmetres( "Smoothing", "##splineriversmoothing", &v.smoothing, 0.0f, 200.0f, "%.0f m" );
				bool bDownhill = v.downhill != 0;
				ImGui::SetCursorPosX( fRowFieldX );
				if ( ImGui::Checkbox( "Downhill Only##splineriverdownhill", &bDownhill ) ) { v.downhill = bDownhill ? 1 : 0; bChanged = true; }
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The bed never rises from the first node to the last: draw from the source to the mouth" );
				spline_rowtexture( "Bed Texture", "##splineriverbedtexture", &v.bedMaterial, w );
				spline_rowtexture( "Bank Texture", "##splineriverbanktexture", &v.bankMaterial, w );
				bChanged |= spline_rowmetres( "Clear Grass", "##splinerivergrass", &v.grassMargin, 0.0f, 30.0f );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Grass is cleared this far past the edge of the bed" );
				bChanged |= spline_rowmetres( "Clear Trees", "##splinerivertrees", &v.treeMargin, 0.0f, 30.0f );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Trees are hidden this far past the edge of the bed" );
				bChanged |= spline_rowmetres( "Water Depth", "##splineriverwaterdepth", &v.waterDepth, 0.0f, std::max( 0.5f, v.depth / SPLINE_UNITS_PER_M ) );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "How deep the river's water is above its bed (0: a dry channel)" );
				bChanged |= spline_rowmetres( "Wade Depth", "##splineriverwadedepth", &v.wadeDepth, 0.0f, 2.0f );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Characters walk through the river where its water is no deeper than this (the navmesh, for paths): a shallow river is crossed, a deep one only along its edges; 0 none of it" );
				bool bRaise = v.raiseBanks != 0;
				ImGui::SetCursorPosX( fRowFieldX );
				if ( ImGui::Checkbox( "Raise Low Banks##splineriverraise", &bRaise ) ) { v.raiseBanks = bRaise ? 1 : 0; bChanged = true; }
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Where the ground beside the river is lower than its water (a dip it crosses, a shallow bank), raise a bank to hold the water; off, the water is lowered or left out there" );
				spline_row( "Rapids" );
				bChanged |= ImGui::SliderFloat( "##splineriverrapids", &v.rapids, 0.0f, 2.0f, "%.2f" );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "White water where the river's slope changes sharply (the top and the foot of a steep run), and a little along a steep run; 0 none" );
				spline_row( "Bank Foam" );
				bChanged |= ImGui::SliderFloat( "##splineriverbankfoam", &v.foam, 0.0f, 1.0f, "%.2f" );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Broken foam where the water meets the banks; 0 none" );
				bool bCalm = v.calmEnd != 0;
				ImGui::SetCursorPosX( fRowFieldX );
				if ( ImGui::Checkbox( "Calm River End##splinerivercalm", &bCalm ) ) { v.calmEnd = bCalm ? 1 : 0; bChanged = true; }
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The rapids fade out over the last 40 m before the river's last node (into a lake); they always fade before the sea" );
				spline_row( "Steep Flow Speed" );
				bChanged |= ImGui::SliderFloat( "##splineriversteepflow", &v.steepFlow, 0.0f, 4.0f, "%.2f" );
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "How much faster the water runs where the river is steep; 0 the same everywhere" );
				bool bMainLook = v.mainLook != 0;
				ImGui::SetCursorPosX( fRowFieldX );
				if ( ImGui::Checkbox( "Use the Main Water's Look##splinerivermainlook", &bMainLook ) )
				{
					if ( !bMainLook )
					{
						// start from the main water's look
						WickedCallWaterLook look = spline_waterlook( v );
						v.colour[0] = look.r; v.colour[1] = look.g; v.colour[2] = look.b;
						v.clarity = 1.0f - look.fogMinAmount; v.seeDepth = look.fogMax;
						v.flow = look.speed; v.waves = look.distortion; v.foam = 1.0f; v.ripples = 1.0f;
					}
					v.mainLook = bMainLook ? 1 : 0;
					bChanged = true;
				}
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The river's water takes its colour, flow and waves from the main water; untick to set them here" );
				if ( !v.mainLook )
				{
					spline_row( "Water Colour" );
					bChanged |= ImGui::ColorEdit3( "##splineriverwatercolour", v.colour, ImGuiColorEditFlags_NoInputs );
					spline_row( "Clarity" );
					bChanged |= ImGui::SliderFloat( "##splineriverclarity", &v.clarity, 0.0f, 1.0f, "%.2f" );
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "How much of the river bed shows through where the water is shallow" );
					bChanged |= spline_rowmetres( "See Depth", "##splineriverseedepth", &v.seeDepth, 0.5f, 400.0f, "%.1f m" );
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "How deep the water gets before its colour hides the bed" );
					spline_row( "Flow Speed" );
					bChanged |= ImGui::SliderFloat( "##splineriverflow", &v.flow, 0.0f, 4.0f, "%.2f" );
					spline_row( "Waves" );
					bChanged |= ImGui::SliderFloat( "##splineriverwaves", &v.waves, 0.0f, 4.0f, "%.2f" );
					spline_row( "Ripple Size" );
					bChanged |= ImGui::SliderFloat( "##splineriverripples", &v.ripples, 0.25f, 4.0f, "%.2f" );
				}
				if ( bChanged ) spline_modified();
				spline_rowbake( s, &v.autoApply, "River", w );
				spline_rowlayers( s, w );
				if ( s.wetFraction >= 0.0f && !s.baked.empty() )
				{
					ImGui::TextWrapped( "Sea: %.0f%% of the bed is below the level's water line (%.1f m).", s.wetFraction * 100.0f, t.terrain.waterliney_f / SPLINE_UNITS_PER_M );
					if ( s.waterEntity && s.waterLowered > 0.005f )
						ImGui::TextWrapped( "The water is lowered along %.0f%% of the river, where a bank is lower than the water: deepen the river or lower Water Depth.", s.waterLowered * 100.0f );
				}
			}
			bool bClosed = s.closed != 0;
			if ( ImGui::Checkbox( "Closed Loop##splineclosed", &bClosed ) && s.nodes.size() > 2 )
			{
				s.closed = bClosed ? 1 : 0;
				spline_modified();
			}
			spline_row( "Pieces" );
			ImGui::SliderInt( "##splinepieces", &g_iSplinePieces, 2, 8 );
			if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "How many pieces Subdivide splits each segment into" );
			if ( ImGui::StyleButton( "Subdivide All##splinesubdivide", ImVec2( w * 0.45f, 0 ) ) )
			{
				for ( int seg = spline_segments( s ) - 1; seg >= 0; seg-- ) spline_subdividesegment( g_iSplineSelected, seg, g_iSplinePieces );
				g_iSplineNodeSelected = -1;
				g_iSplineSegSelected = -1;
			}
			if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Split every segment of the spline" );
			ImGui::SameLine();
			if ( ImGui::StyleButton( "Subdivide Segment##splinesubdivideseg", ImVec2( w * 0.45f, 0 ) ) && g_iSplineSegSelected >= 0 )
			{
				spline_subdividesegment( g_iSplineSelected, g_iSplineSegSelected, g_iSplinePieces );
				g_iSplineNodeSelected = -1;
				g_iSplineSegSelected = -1;
			}
			if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", g_iSplineSegSelected >= 0 ? "Split the highlighted segment between its two nodes" : "Click the spline's curve first to pick the segment between two nodes" );
			if ( g_iSplineSegSelected >= 0 && g_iSplineSegSelected < (int)s.nodes.size() )
			{
				// the picked segment's own curve; a Bezier one starts from the shape it has now
				const char* segCurves[] = { "As the Spline", "Straight", "Smooth", "Bezier" };
				int segCurve = s.nodes[ g_iSplineSegSelected ].segCurve + 1;
				spline_row( "Segment Curve" );
				if ( ImGui::Combo( "##splinesegcurve", &segCurve, segCurves, 4 ) && segCurve - 1 != s.nodes[ g_iSplineSegSelected ].segCurve )
				{
					const int seg = g_iSplineSegSelected;
					const int ib = spline_wrap( s, seg + 1 );
					float c[8];
					spline_controls( s, seg, c );
					s.nodes[ seg ].segCurve = segCurve - 1;
					if ( spline_segcurve( s, seg ) == SPLINE_CURVE_BEZIER )
					{
						s.nodes[ seg ].outX = c[2] - c[0]; s.nodes[ seg ].outZ = c[3] - c[1];
						s.nodes[ ib ].inX = c[4] - c[6]; s.nodes[ ib ].inZ = c[5] - c[7];
					}
					spline_modified();
				}
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The curve of the highlighted segment alone; the rest keep the spline's Curve" );
				if ( s.kind == SPLINE_KIND_ROAD )
				{
					const char* segMarks[] = { "As the Road", "None", "Centre Solid", "Centre Dashed" };
					int segMark = s.nodes[ g_iSplineSegSelected ].segMark + 1;
					spline_row( "Segment Markings" );
					if ( ImGui::Combo( "##splinesegmark", &segMark, segMarks, 4 ) && segMark - 1 != s.nodes[ g_iSplineSegSelected ].segMark )
					{
						s.nodes[ g_iSplineSegSelected ].segMark = segMark - 1;
						spline_modified();
					}
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The highlighted segment's own markings: none (a village street), or its centre line solid (a bridge, a crest) or dashed" );
				}
			}
			if ( ImGui::StyleButton( "Reverse##splinereverse", ImVec2( w * 0.45f, 0 ) ) )
			{
				spline_reverse( s );
				g_iSplineNodeSelected = -1;
				g_iSplineSegSelected = -1;
				spline_modified();
			}
			ImGui::Text( "%d nodes, %.0f m", (int)s.nodes.size(), spline_length( s ) / 39.37f );

			if ( g_iSplineNodeSelected >= 0 )
			{
				sSplineNode& node = s.nodes[ g_iSplineNodeSelected ];
				ImGui::Separator();
				ImGui::Text( "Node %d of %d", g_iSplineNodeSelected + 1, (int)s.nodes.size() );
				if ( node.junction )
				{
					int others = -1;
					for ( sSpline& other : g_Splines )
						for ( sSplineNode& n : other.nodes )
							if ( n.junction == node.junction ) others++;
					ImGui::Text( "Joined with %d other node%s", others, others == 1 ? "" : "s" );
					if ( ImGui::StyleButton( "Detach##splinedetach", ImVec2( w * 0.45f, 0 ) ) )
					{
						node.junction = 0;
						spline_cleanjunctions();
						spline_modified();
					}
					// an end joined to another spline's end can become one spline through that point
					if ( spline_isend( s, g_iSplineNodeSelected ) )
					{
						for ( int sj = 0; sj < (int)g_Splines.size(); sj++ )
						{
							if ( sj == g_iSplineSelected ) continue;
							const sSpline& other = g_Splines[ sj ];
							for ( int nj = 0; nj < (int)other.nodes.size(); nj++ )
							{
								if ( other.nodes[ nj ].junction != node.junction || !spline_isend( other, nj ) ) continue;
								char label[ 128 ];
								sprintf_s( label, 128, "Merge with %s##splinemerge%d", other.name, other.id );
								if ( ImGui::StyleButton( label, ImVec2( w * 0.92f, 0 ) ) )
								{
									spline_merge( g_iSplineSelected, g_iSplineNodeSelected, sj, nj, true );
									sj = (int)g_Splines.size();
									break;
								}
							}
						}
					}
				}
				if ( g_iSplineSelected >= 0 && g_iSplineNodeSelected >= 0 && spline_isend( g_Splines[ g_iSplineSelected ], g_iSplineNodeSelected ) )
				{
					if ( ImGui::StyleButton( bConnectMode ? "Click another spline's end...##splineconnect" : "Connect to Another End##splineconnect", ImVec2( w * 0.92f, 0 ) ) )
					{
						bConnectMode = !bConnectMode;
						g_bSplineEditMode = true;
					}
					if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Join this end to another spline's end with a new segment between them, making one spline" );
				}
				if ( g_iSplineSelected >= 0 && g_iSplineNodeSelected >= 0 )
				{
					sSpline& sel = g_Splines[ g_iSplineSelected ];
					const bool bNodeBezier = spline_handleshown( sel, g_iSplineNodeSelected, 1 ) || spline_handleshown( sel, g_iSplineNodeSelected, 2 );
					if ( bNodeBezier && ImGui::StyleButton( "Smooth Handles##splinesmoothhandles", ImVec2( w * 0.45f, 0 ) ) )
					{
						sSpline smooth = sel;
						smooth.curve = SPLINE_CURVE_SMOOTH;
						for ( sSplineNode& sn : smooth.nodes ) sn.segCurve = -1;
						sSplineNode& n = sel.nodes[ g_iSplineNodeSelected ];
						spline_handles( smooth, g_iSplineNodeSelected, &n.inX, &n.inZ, &n.outX, &n.outZ );
						n.flags &= ~SPLINE_NODE_BROKEN;
						spline_modified();
					}
					if ( bNodeBezier ) ImGui::SameLine();
					if ( ImGui::StyleButton( "Delete Node##splinedeletenode", ImVec2( w * 0.45f, 0 ) ) )
					{
						spline_deletenode( g_iSplineSelected, g_iSplineNodeSelected );
					}
				}
			}
		}
		ImGui::Indent( -10 );
	}

	if ( bSectionOpen )
	{
		// the terrain's own tools leave the mouse alone meanwhile, and the splines are shown
		GGTerrain::ggterrain_extra_params.edit_mode = GGTERRAIN_EDIT_NONE;
		GGTerrain::ggterrain_global_render_params2.flags2 &= ~GGTERRAIN_SHADER_FLAG2_SHOW_BRUSH_SIZE;
		if ( g_bSplineEditMode ) spline_draw();
	}
	if ( g_bSplineEditMode )
	{
		if ( bDrawing && ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Escape ) ) ) { bDrawing = false; g_iSplineNodeSelected = -1; }
		spline_mouse();
	}
	else
	{
		iDragSpline = iDragNode = -1;
		iDragHandle = 0;
		bConnectMode = false;
		bDrawing = false;
	}

	spline_bakechanged();
	spline_updatewater();
	spline_placechanged();
	spline_undocommit();
}

//
// map.spl
//

void spline_deleteall( void )
{
	for ( sSpline& s : g_Splines ) if ( s.waterEntity ) WickedCall_DeleteWaterSurface( s.waterEntity );
	g_RiverWater.clear();
	g_Splines.clear();
	g_iSplineSelected = -1;
	g_iSplineNodeSelected = -1;
	g_iSplineNextID = 1;
	g_iSplineNextJunction = 1;
	iDragSpline = iDragNode = -1;
	iDragHandle = 0;
	bConnectMode = false;
	// a new or loaded level: the steps on the undo list are another level's
	g_iSplineUndoGeneration++;
	g_bSplineUndoBaseSet = false;
	g_SplineUndoBase.clear();
	g_SplineForceBake.clear();
	g_SplineMarkings.clear();
	g_SplineMarkingsApplied = g_SplineMarkingsPending = 0;
	GGTerrain::GGTerrain_SetMarkings( nullptr, 0 );
}

void spline_savedata( void )
{
	char pPath[ MAX_PATH ];
	strcpy_s( pPath, MAX_PATH, (g.mysystem.levelBankTestMap_s + "map.spl").Get() );
	GG_GetRealPath( pPath, 1 );
	if ( FileExist( pPath ) == 1 ) DeleteAFile( pPath );
	if ( g_Splines.empty() ) return;
	FILE* fp = NULL;
	if ( fopen_s( &fp, pPath, "wb" ) != 0 || !fp ) return;
	const uint32_t header[3] = { SPLINE_FILE_MAGIC, SPLINE_FILE_VERSION, (uint32_t)g_Splines.size() };
	fwrite( header, sizeof(header), 1, fp );
	std::vector<uint8_t> record;
	auto put = [&record]( const void* p, size_t bytes ) { const uint8_t* b = (const uint8_t*)p; record.insert( record.end(), b, b + bytes ); };
	for ( const sSpline& s : g_Splines )
	{
		record.clear();
		const int32_t values[4] = { s.id, s.kind, s.curve, s.closed };
		put( values, sizeof(values) );
		put( s.name, 64 );
		const uint32_t nodeCount = (uint32_t)s.nodes.size();
		put( &nodeCount, sizeof(nodeCount) );
		for ( const sSplineNode& node : s.nodes )
		{
			const float f[7] = { node.x, node.y, node.z, node.inX, node.inZ, node.outX, node.outZ };
			const int32_t i[2] = { node.flags, node.junction };
			put( f, sizeof(f) );
			put( i, sizeof(i) );
		}
		// version 2: the road's settings, and the bake
		const sSplineRoad& r = s.road;
		const float rf[7] = { r.width, r.shoulder, r.smoothing, r.maxGrade, r.crown, r.grassMargin, r.treeMargin };
		const int32_t ri[3] = { r.material, r.edgeMaterial, r.autoApply };
		put( rf, sizeof(rf) );
		put( ri, sizeof(ri) );
		put( &s.bakedSignature, sizeof(s.bakedSignature) );
		const uint32_t texels = (uint32_t)s.baked.size();
		put( &texels, sizeof(texels) );
		for ( const sSplineBakeTexel& b : s.baked )
		{
			const uint16_t xz[2] = { b.x, b.z };
			const uint8_t bytes[8] = { b.flags, b.typeBefore, b.typeAfter, b.matBefore, b.matAfter, b.grassBefore, b.grassAfter, 0 };
			const float heights[2] = { b.heightBefore, b.heightAfter };
			put( xz, sizeof(xz) );
			put( bytes, sizeof(bytes) );
			put( heights, sizeof(heights) );
		}
		const uint32_t trees = (uint32_t)s.bakedTrees.size();
		put( &trees, sizeof(trees) );
		for ( const sSplineBakeTree& tree : s.bakedTrees )
		{
			const float xz[2] = { tree.x, tree.z };
			put( &tree.id, sizeof(tree.id) );
			put( xz, sizeof(xz) );
			put( &tree.data, sizeof(tree.data) );
		}
		// version 3: the river's settings
		const sSplineRiver& v = s.river;
		const float vf[6] = { v.bedWidth, v.depth, v.banks, v.smoothing, v.grassMargin, v.treeMargin };
		const int32_t vi[4] = { v.bedMaterial, v.bankMaterial, v.downhill, v.autoApply };
		put( vf, sizeof(vf) );
		put( vi, sizeof(vi) );
		// version 4: the river's water
		const float wf[9] = { v.waterDepth, v.colour[0], v.colour[1], v.colour[2], v.clarity, v.flow, v.waves, v.foam, v.ripples };
		const int32_t wi[1] = { v.mainLook };
		put( wf, sizeof(wf) );
		put( wi, sizeof(wi) );
		// version 5: Raise Low Banks
		put( &v.raiseBanks, sizeof(v.raiseBanks) );
		// version 6: See Depth
		put( &v.seeDepth, sizeof(v.seeDepth) );
		// version 7: rapids
		const float rapids[2] = { v.rapids, v.steepFlow };
		put( rapids, sizeof(rapids) );
		// version 9: placement layers, what they placed, and for which layers and bake
		const uint32_t layerCount = (uint32_t)s.layers.size();
		put( &layerCount, sizeof(layerCount) );
		for ( const sSplineLayer& layer : s.layers )
		{
			put( layer.entity, 260 );
			const float lf[9] = { layer.spacing, layer.start, layer.offset, layer.turn, layer.height, layer.scaleMin, layer.scaleMax, layer.jitter, layer.minTurbulence };
			const int32_t li[3] = { layer.side, layer.facing, layer.enabled };
			put( lf, sizeof(lf) );
			put( li, sizeof(li) );
		}
		const uint32_t placedCount = (uint32_t)s.placed.size();
		put( &placedCount, sizeof(placedCount) );
		for ( const sSplinePlaced& placed : s.placed )
		{
			const int32_t pl = placed.layer;
			const float pf[3] = { placed.x, placed.y, placed.z };
			put( &pl, sizeof(pl) );
			put( pf, sizeof(pf) );
		}
		put( &s.placedSignature, sizeof(s.placedSignature) );
		// version 10: the layers' names, keep apart and freeze; Calm River End
		for ( const sSplineLayer& layer : s.layers )
		{
			put( layer.name, 64 );
			put( &layer.keepApart, sizeof(layer.keepApart) );
			put( &layer.frozen, sizeof(layer.frozen) );
		}
		put( &v.calmEnd, sizeof(v.calmEnd) );
		// version 11: Follow Slope
		for ( const sSplineLayer& layer : s.layers ) put( &layer.followSlope, sizeof(layer.followSlope) );
		// version 13: Jitter Across
		for ( const sSplineLayer& layer : s.layers ) put( &layer.jitterAcross, sizeof(layer.jitterAcross) );
		// version 14: the preset
		put( s.preset, 64 );
		put( &s.presetSignature, sizeof(s.presetSignature) );
		// version 15: each segment's curve
		for ( const sSplineNode& node : s.nodes ) put( &node.segCurve, sizeof(node.segCurve) );
		// version 16: Lay Flat
		for ( const sSplineLayer& layer : s.layers ) put( &layer.layFlat, sizeof(layer.layFlat) );
		// version 17: Wade Depth
		put( &v.wadeDepth, sizeof(v.wadeDepth) );
		// version 18: the road's markings, each segment's
		const float mf[7] = { r.markEdgeInset, r.markLineWidth, r.markEdgeWidth, r.markDash, r.markGap, r.markBendRadius, r.markWear };
		const int32_t mi[5] = { r.markCentre, r.markCentreYellow, r.markLanes, r.markEdges, r.markEdgeYellow };
		put( mf, sizeof(mf) );
		put( mi, sizeof(mi) );
		for ( const sSplineNode& node : s.nodes ) put( &node.segMark, sizeof(node.segMark) );
		const uint32_t bytes = (uint32_t)record.size();
		fwrite( &bytes, sizeof(bytes), 1, fp );
		if ( bytes ) fwrite( record.data(), bytes, 1, fp );
	}
	fclose( fp );
}

void spline_loaddata( void )
{
	spline_deleteall();
	if ( ImGui::GetCurrentContext() ) g_iSplineLoadFrame = ImGui::GetFrameCount();
	char pPath[ MAX_PATH ];
	strcpy_s( pPath, MAX_PATH, (g.mysystem.levelBankTestMap_s + "map.spl").Get() );
	GG_GetRealPath( pPath, 0 );
	FILE* fp = NULL;
	if ( fopen_s( &fp, pPath, "rb" ) != 0 || !fp ) return;
	uint32_t header[3] = { 0, 0, 0 };
	if ( fread( header, sizeof(header), 1, fp ) != 1 || header[0] != SPLINE_FILE_MAGIC || header[1] < 1 )
	{
		fclose( fp );
		return;
	}
	const uint32_t version = header[1];
	std::vector<uint8_t> record;
	for ( uint32_t k = 0; k < header[2]; k++ )
	{
		uint32_t bytes = 0;
		if ( fread( &bytes, sizeof(bytes), 1, fp ) != 1 || bytes > 512u * 1024u * 1024u ) break;
		record.resize( bytes );
		if ( bytes && fread( record.data(), bytes, 1, fp ) != 1 ) break;
		size_t at = 0;
		auto get = [&record, &at]( void* p, size_t n ) { if ( at + n > record.size() ) return false; memcpy( p, record.data() + at, n ); at += n; return true; };

		sSpline s;
		int32_t values[4] = { 0, 0, 0, 0 };
		uint32_t nodeCount = 0;
		if ( !get( values, sizeof(values) ) || !get( s.name, 64 ) || !get( &nodeCount, sizeof(nodeCount) ) ) continue;
		s.name[ 63 ] = 0;
		s.id = values[0]; s.kind = values[1]; s.curve = values[2]; s.closed = values[3];
		bool bOK = true;
		for ( uint32_t n = 0; n < nodeCount && bOK; n++ )
		{
			float f[7];
			int32_t i[2];
			if ( !get( f, sizeof(f) ) || !get( i, sizeof(i) ) ) { bOK = false; break; }
			sSplineNode node;
			node.x = f[0]; node.y = f[1]; node.z = f[2]; node.inX = f[3]; node.inZ = f[4]; node.outX = f[5]; node.outZ = f[6];
			node.flags = i[0]; node.junction = i[1];
			s.nodes.push_back( node );
			if ( node.junction >= g_iSplineNextJunction ) g_iSplineNextJunction = node.junction + 1;
		}
		if ( !bOK ) continue;
		if ( version >= 2 )
		{
			float rf[7];
			int32_t ri[3];
			uint32_t texels = 0, trees = 0;
			if ( get( rf, sizeof(rf) ) && get( ri, sizeof(ri) ) && get( &s.bakedSignature, sizeof(s.bakedSignature) ) && get( &texels, sizeof(texels) ) )
			{
				sSplineRoad& r = s.road;
				r.width = rf[0]; r.shoulder = rf[1]; r.smoothing = rf[2]; r.maxGrade = rf[3]; r.crown = rf[4]; r.grassMargin = rf[5]; r.treeMargin = rf[6];
				r.material = ri[0]; r.edgeMaterial = ri[1]; r.autoApply = ri[2];
				s.baked.reserve( texels );
				for ( uint32_t t = 0; t < texels; t++ )
				{
					uint16_t xz[2];
					uint8_t b8[8];
					float heights[2];
					if ( !get( xz, sizeof(xz) ) || !get( b8, sizeof(b8) ) || !get( heights, sizeof(heights) ) ) break;
					sSplineBakeTexel b;
					b.x = xz[0]; b.z = xz[1];
					b.flags = b8[0]; b.typeBefore = b8[1]; b.typeAfter = b8[2]; b.matBefore = b8[3]; b.matAfter = b8[4]; b.grassBefore = b8[5]; b.grassAfter = b8[6];
					b.heightBefore = heights[0]; b.heightAfter = heights[1];
					s.baked.push_back( b );
				}
				if ( get( &trees, sizeof(trees) ) )
				{
					for ( uint32_t t = 0; t < trees; t++ )
					{
						sSplineBakeTree tree;
						float xz[2];
						if ( !get( &tree.id, sizeof(tree.id) ) || !get( xz, sizeof(xz) ) || !get( &tree.data, sizeof(tree.data) ) ) break;
						tree.x = xz[0]; tree.z = xz[1];
						s.bakedTrees.push_back( tree );
					}
				}
				float vf[6];
				int32_t vi[4];
				if ( version >= 3 && get( vf, sizeof(vf) ) && get( vi, sizeof(vi) ) )
				{
					sSplineRiver& v = s.river;
					v.bedWidth = vf[0]; v.depth = vf[1]; v.banks = vf[2]; v.smoothing = vf[3]; v.grassMargin = vf[4]; v.treeMargin = vf[5];
					v.bedMaterial = vi[0]; v.bankMaterial = vi[1]; v.downhill = vi[2]; v.autoApply = vi[3];
				}
				float wf[9];
				int32_t wi[1];
				if ( version >= 4 && get( wf, sizeof(wf) ) && get( wi, sizeof(wi) ) )
				{
					sSplineRiver& v = s.river;
					v.waterDepth = wf[0]; v.colour[0] = wf[1]; v.colour[1] = wf[2]; v.colour[2] = wf[3];
					v.clarity = wf[4]; v.flow = wf[5]; v.waves = wf[6]; v.foam = wf[7]; v.ripples = wf[8];
					v.mainLook = wi[0];
				}
				int32_t raise = 1;
				if ( version >= 5 && get( &raise, sizeof(raise) ) ) s.river.raiseBanks = raise;
				float seeDepth = 0;
				if ( version >= 6 && get( &seeDepth, sizeof(seeDepth) ) ) s.river.seeDepth = seeDepth;
				else if ( version < 6 ) s.river.clarity = 0.75f; // before version 6 clarity was the opacity
				float rapids[2];
				if ( version >= 7 && get( rapids, sizeof(rapids) ) ) { s.river.rapids = rapids[0]; s.river.steepFlow = rapids[1]; }
				if ( version < 8 ) s.river.foam = 0.3f; // before version 8 the foam was the Water Object's, and 1
				uint32_t layerCount = 0;
				if ( version >= 9 && get( &layerCount, sizeof(layerCount) ) && layerCount < 1000 )
				{
					for ( uint32_t li = 0; li < layerCount; li++ )
					{
						sSplineLayer layer;
						float lf[9];
						int32_t lv[3];
						if ( !get( layer.entity, 260 ) || !get( lf, sizeof(lf) ) || !get( lv, sizeof(lv) ) ) break;
						layer.entity[ 259 ] = 0;
						layer.spacing = lf[0]; layer.start = lf[1]; layer.offset = lf[2]; layer.turn = lf[3]; layer.height = lf[4];
						layer.scaleMin = lf[5]; layer.scaleMax = lf[6]; layer.jitter = lf[7]; layer.minTurbulence = lf[8];
						layer.jitterAcross = layer.jitter; // before version 13 one jitter moved it both ways
						layer.side = lv[0]; layer.facing = lv[1]; layer.enabled = lv[2];
						s.layers.push_back( layer );
					}
					uint32_t placedCount = 0;
					if ( get( &placedCount, sizeof(placedCount) ) && placedCount < 1000000 )
					{
						for ( uint32_t pi = 0; pi < placedCount; pi++ )
						{
							int32_t pl = 0;
							float pf[3];
							if ( !get( &pl, sizeof(pl) ) || !get( pf, sizeof(pf) ) ) break;
							sSplinePlaced placed;
							placed.layer = pl; placed.x = pf[0]; placed.y = pf[1]; placed.z = pf[2];
							s.placed.push_back( placed );
						}
					}
					get( &s.placedSignature, sizeof(s.placedSignature) );
					if ( version >= 10 )
					{
						for ( sSplineLayer& layer : s.layers )
						{
							if ( !get( layer.name, 64 ) || !get( &layer.keepApart, sizeof(layer.keepApart) ) || !get( &layer.frozen, sizeof(layer.frozen) ) ) break;
							layer.name[ 63 ] = 0;
						}
						get( &s.river.calmEnd, sizeof(s.river.calmEnd) );
						if ( version >= 11 )
						{
							for ( sSplineLayer& layer : s.layers ) if ( !get( &layer.followSlope, sizeof(layer.followSlope) ) ) break;
						}
						if ( version >= 13 )
						{
							for ( sSplineLayer& layer : s.layers ) if ( !get( &layer.jitterAcross, sizeof(layer.jitterAcross) ) ) break;
						}
						if ( version >= 14 && get( s.preset, 64 ) )
						{
							s.preset[ 63 ] = 0;
							get( &s.presetSignature, sizeof(s.presetSignature) );
						}
						if ( version >= 15 )
						{
							for ( sSplineNode& node : s.nodes ) if ( !get( &node.segCurve, sizeof(node.segCurve) ) ) break;
						}
						if ( version >= 16 )
						{
							for ( sSplineLayer& layer : s.layers ) if ( !get( &layer.layFlat, sizeof(layer.layFlat) ) ) break;
						}
						if ( version >= 17 ) get( &s.river.wadeDepth, sizeof(s.river.wadeDepth) );
						if ( version >= 18 )
						{
							float mf[7];
							int32_t mi[5];
							if ( get( mf, sizeof(mf) ) && get( mi, sizeof(mi) ) )
							{
								sSplineRoad& r = s.road;
								r.markEdgeInset = mf[0]; r.markLineWidth = mf[1]; r.markEdgeWidth = mf[2]; r.markDash = mf[3];
								r.markGap = mf[4]; r.markBendRadius = mf[5]; r.markWear = mf[6];
								r.markCentre = mi[0]; r.markCentreYellow = mi[1]; r.markLanes = mi[2]; r.markEdges = mi[3]; r.markEdgeYellow = mi[4];
								for ( sSplineNode& node : s.nodes ) if ( !get( &node.segMark, sizeof(node.segMark) ) ) break;
							}
						}
						if ( version < 18 && s.preset[0] && s.presetSignature == spline_presetsignature( s, version < 17 ? offsetof( sSplineRiver, wadeDepth ) : sizeof(sSplineRiver), version < 16 ? offsetof( sSplineLayer, layFlat ) : sizeof(sSplineLayer), offsetof( sSplineRoad, markCentre ) ) )
						{
							// still as its preset set it, by the settings that file had: so too by today's (Lay Flat, Wade
							// Depth and the road's markings added since), so it doesn't show as edited
							s.presetSignature = spline_presetsignature( s );
						}
						if ( version < 12 )
						{
							// versions 10 and 11 named a road's new layer <road>_streetlight: that layer takes its entity's name now
							char oldName[ 64 ];
							int j = 0;
							for ( const char* p = s.name; *p && j < 40; p++ ) oldName[ j++ ] = isalnum( (unsigned char)*p ) ? *p : '_';
							oldName[ j ] = 0;
							strcat_s( oldName, 64, "_streetlight" );
							for ( sSplineLayer& layer : s.layers ) if ( _stricmp( layer.name, oldName ) == 0 ) layer.name[0] = 0;
						}
					}
					else
					{
						// before version 10 a road's layers keep apart as a new one does, a river's not
						for ( sSplineLayer& layer : s.layers ) layer.keepApart = s.kind == SPLINE_KIND_ROAD ? 600.0f : 0.0f;
					}
				}
			}
		}
		else
		{
			// before version 2 nothing was baked: a spline as it was saved needs no bake
			s.bakedSignature = spline_signature( s );
		}
		if ( s.id >= g_iSplineNextID ) g_iSplineNextID = s.id + 1;
		g_Splines.push_back( s );
	}
	fclose( fp );
	spline_cleanjunctions();
	g_iSplineWaterWait = 60;
}
