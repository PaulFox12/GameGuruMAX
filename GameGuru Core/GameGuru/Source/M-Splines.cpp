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
#define SPLINE_FILE_VERSION 8 // 2: road settings and the bake; 3: river settings; 4: the river's water; 5: Raise Low Banks; 6: See Depth; 7: rapids; 8: Bank Foam

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
};

// a river's water over a terrain texel: its height, and how turbulent it is there (0-1)
struct sRiverTexel
{
	float height = 0;
	float turbulence = 0;
	float flowX = 0, flowZ = 0; // the current, units a second downstream
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
static void spline_handles( const sSpline& s, int i, float* pInX, float* pInZ, float* pOutX, float* pOutZ )
{
	*pInX = *pInZ = *pOutX = *pOutZ = 0;
	const int n = (int)s.nodes.size();
	const sSplineNode& p = s.nodes[ i ];
	if ( s.curve == SPLINE_CURVE_BEZIER )
	{
		*pInX = p.inX; *pInZ = p.inZ; *pOutX = p.outX; *pOutZ = p.outZ;
		return;
	}
	if ( s.curve != SPLINE_CURVE_SMOOTH || n < 2 ) return;
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
	*pInX = -dx * before; *pInZ = -dz * before;
	*pOutX = dx * after; *pOutZ = dz * after;
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
	if ( s.curve == SPLINE_CURVE_LINEAR )
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
	if ( s.curve == SPLINE_CURVE_BEZIER )
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

static void spline_deletespline( int si )
{
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
	std::reverse( s.nodes.begin(), s.nodes.end() );
	for ( sSplineNode& node : s.nodes )
	{
		std::swap( node.inX, node.outX );
		std::swap( node.inZ, node.outZ );
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
// segment runs between them
static void spline_merge( int si, int ni, int sj, int nj, bool bShared )
{
	if ( si == sj ) return;
	spline_unbake( si );
	spline_unbake( sj );
	sSpline& a = g_Splines[ si ];
	sSpline b = g_Splines[ sj ];
	if ( a.curve == SPLINE_CURVE_BEZIER && b.curve != SPLINE_CURVE_BEZIER ) spline_seedhandles( b, b.curve );
	if ( ni == 0 && a.nodes.size() > 1 ) spline_reverse( a );
	if ( nj != 0 ) spline_reverse( b );
	size_t first = 0;
	if ( bShared && !b.nodes.empty() )
	{
		sSplineNode& joint = a.nodes.back();
		joint.outX = b.nodes[0].outX;
		joint.outZ = b.nodes[0].outZ;
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
	float d, h;
	int k;
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
				if ( dist > reach ) continue;
				const uint32_t key = (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix;
				auto it = foot.find( key );
				if ( it == foot.end() || dist < it->second.d ) foot[ key ] = { dist, a.h + (b.h - a.h) * t, k };
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
		if ( pM )
		{
			if ( bCore && shape.coreMaterial > 0 ) { pM[ mIndex ] = (uint8_t)shape.coreMaterial; b.flags |= SPLINE_TEXEL_MATERIAL; }
			else if ( !bCore && !bProtected && shape.edgeMaterial > 0 && f.d <= halfW + shape.edge ) { pM[ mIndex ] = (uint8_t)shape.edgeMaterial; b.flags |= SPLINE_TEXEL_MATERIAL; }
		}
		if ( pG && f.d <= halfW + shape.grassMargin && !bProtected )
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
	if ( !GGTerrain::GGTerrain_IsReady() ) return;
	const size_t count = g_Splines.size();
	std::vector<char> in( count, 0 );
	bool bAny = false;
	for ( size_t si = 0; si < count; si++ )
	{
		const sSpline& s = g_Splines[ si ];
		const bool bChanged = spline_signature( s ) != s.bakedSignature;
		bool bBake = false;
		if ( s.kind == SPLINE_KIND_ROAD ) bBake = (bChanged && s.road.autoApply) || (int)si == iApplySpline;
		else if ( s.kind == SPLINE_KIND_RIVER ) bBake = (bChanged && s.river.autoApply) || (int)si == iApplySpline;
		else bBake = !s.baked.empty() || !s.bakedTrees.empty();
		if ( bBake ) { in[ si ] = 1; bAny = true; }
	}
	iApplySpline = -1;
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

// for the navmesh bake: the rivers' water height at a point (none: -1e30), and a hash of their water over a rect (0 none)
float spline_riverwaterlevel( float x, float z )
{
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( g_RiverWater.empty() || E <= 0 ) return -1e30f;
	const int ix = (int)((x / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f), iz = (int)((z / E * 0.5f + 0.5f) * SPLINE_MAP_SIZE + 0.5f);
	if ( ix < 0 || iz < 0 || ix >= SPLINE_MAP_SIZE || iz >= SPLINE_MAP_SIZE ) return -1e30f;
	auto it = g_RiverWater.find( (uint32_t)iz * SPLINE_MAP_SIZE + (uint32_t)ix );
	return it != g_RiverWater.end() ? it->second.height : -1e30f;
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
		for ( const auto& texel : g_RiverWater ) all.push_back( { texel.first, texel.second.height } );
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
				h = spline_hashmix( h, &key, sizeof(key) );
				h = spline_hashmix( h, &it->second.height, sizeof(it->second.height) );
				bAny = true;
			}
		}
	}
	return bAny ? (h ? h : 1) : 0;
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
static bool spline_pickhandle( ImVec2 mouse, float radius, int* pNode, int* pHandle )
{
	if ( g_iSplineSelected < 0 || g_Splines[ g_iSplineSelected ].curve != SPLINE_CURVE_BEZIER ) return false;
	const sSpline& s = g_Splines[ g_iSplineSelected ];
	float best = radius;
	bool bFound = false;
	for ( int ni = 0; ni < (int)s.nodes.size(); ni++ )
	{
		for ( int h = 1; h <= 2; h++ )
		{
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
			if ( bSelected && s.curve == SPLINE_CURVE_BEZIER )
			{
				for ( int h = 1; h <= 2; h++ )
				{
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

static void spline_mouse( void )
{
	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 mouse = io.MousePos;
	const bool bInView = bImGuiRenderTargetFocus && !bImGuiGotFocus
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
		if ( io.MouseDown[0] )
		{
			if ( spline_terrainpick( &x, &y, &z ) )
			{
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
	spline_modified();
}

//
// The panel (Terrain Tools) and the editing in the 3D view
//

bool spline_iseditmode( void )
{
	return g_bSplineEditMode;
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
	return ImageExist( image ) == 1;
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
		if ( ImGui::Checkbox( "Edit Splines##splineeditmode", &g_bSplineEditMode ) )
		{
			bConnectMode = false;
		}
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Click the terrain to add nodes and drag them. Shift+click the curve inserts a node, Ctrl+click deletes one.\nA dragged node snaps to another spline's node or curve and joins it (a T junction or a crossing); Alt+drag pulls it out again.\nAn end dropped on the spline's other end closes it. Alt+drag a Bezier handle to break the pair." );

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
			if ( ImGui::Combo( "##splinekind", &s.kind, kinds, 3 ) ) spline_modified();

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
				if ( bChanged ) spline_modified();
				spline_rowbake( s, &r.autoApply, "Road", w );
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
					if ( sel.curve == SPLINE_CURVE_BEZIER && ImGui::StyleButton( "Smooth Handles##splinesmoothhandles", ImVec2( w * 0.45f, 0 ) ) )
					{
						sSpline smooth = sel;
						smooth.curve = SPLINE_CURVE_SMOOTH;
						sSplineNode& n = sel.nodes[ g_iSplineNodeSelected ];
						spline_handles( smooth, g_iSplineNodeSelected, &n.inX, &n.inZ, &n.outX, &n.outZ );
						n.flags &= ~SPLINE_NODE_BROKEN;
						spline_modified();
					}
					if ( sel.curve == SPLINE_CURVE_BEZIER ) ImGui::SameLine();
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
		spline_draw();
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
		const uint32_t bytes = (uint32_t)record.size();
		fwrite( &bytes, sizeof(bytes), 1, fp );
		if ( bytes ) fwrite( record.data(), bytes, 1, fp );
	}
	fclose( fp );
}

void spline_loaddata( void )
{
	spline_deleteall();
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
