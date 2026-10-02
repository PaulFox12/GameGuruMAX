//----------------------------------------------------
//--- GAMEGURU - M-Splines
//----------------------------------------------------

// GG: splines drawn on the terrain in the editor, edited in the 3D view while Edit Splines is on in Terrain Tools (Roads
// and Rivers). A spline is a list of nodes on the ground joined straight, smoothly (each node's tangent along its
// neighbours, each handle a third of its segment) or by Bezier handles. Nodes of different splines can share a junction
// (a T junction or a crossing): they keep one position, so dragging one drags them all. Saved with the level in map.spl
//
// In the 3D view:
// - click the terrain to add a node at the end (or the start, with the first node selected), and drag it while held
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
// surface there, and a later road's shoulders leave an earlier road's carriageway alone

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

#define SPLINE_CURVE_LINEAR 0
#define SPLINE_CURVE_SMOOTH 1
#define SPLINE_CURVE_BEZIER 2

#define SPLINE_KIND_NONE 0
#define SPLINE_KIND_ROAD 1
#define SPLINE_KIND_RIVER 2

#define SPLINE_NODE_BROKEN 1 // the node's Bezier handles move apart

#define SPLINE_FILE_MAGIC 0x50534747 // 'GGSP'
#define SPLINE_FILE_VERSION 2 // 2: road settings and the bake

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
	uint64_t bakedSignature = 0; // the spline as last baked (spline_signature)
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

static void spline_deletespline( int si )
{
	spline_unbake( si );
	g_Splines.erase( g_Splines.begin() + si );
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
	g_Splines.erase( g_Splines.begin() + sj );
	if ( si > sj ) si--;
	g_iSplineSelected = si;
	g_iSplineNodeSelected = -1;
	spline_cleanjunctions();
	spline_modified();
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

// the road profile, the footprint on the terrain's texels, and the writes
static void spline_bakeroad( sSpline& sp, std::unordered_set<uint32_t>& protectedTexels, float* pBounds )
{
	const sSplineRoad& r = sp.road;
	float* pH = GGTerrain::GGTerrain_GetHeightEditMap();
	uint8_t* pT = GGTerrain::GGTerrain_GetHeightEditTypeMap();
	uint8_t* pM = GGTerrain::GGTerrain_GetMaterialMap();
	uint8_t* pG = GGGrass::GGGrass_GetGrassMap();
	const int segs = spline_segments( sp );
	const float E = GGTerrain::GGTerrain_GetEditableSize();
	if ( !pH || !pT || segs == 0 || E <= 0 || r.width <= 0 ) return;
	const float texel = E * 2.0f / SPLINE_MAP_SIZE;
	const float halfW = r.width * 0.5f;

	// the centre line about a quarter texel apart, and the sample at each node
	std::vector<sRoadSample> c;
	std::vector<int> nodeSample( sp.nodes.size(), -1 );
	const float spacing = texel * 0.25f;
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
	{
		sRoadSample p;
		spline_point( sp, segs - 1, 1.0f, &p.x, &p.z );
		const int last = spline_wrap( sp, segs );
		if ( nodeSample[ last ] < 0 ) nodeSample[ last ] = (int)c.size();
		c.push_back( p );
	}
	const int n = (int)c.size();
	if ( n < 2 ) return;
	c[0].s = 0;
	for ( int i = 1; i < n; i++ ) c[i].s = c[i-1].s + sqrtf( (c[i].x - c[i-1].x) * (c[i].x - c[i-1].x) + (c[i].z - c[i-1].z) * (c[i].z - c[i-1].z) );

	// the ground along it (an earlier road's surface included), averaged over the smoothing length
	for ( sRoadSample& p : c )
	{
		if ( !GGTerrain::GGTerrain_GetHeight( p.x, p.z, &p.ground, 1, 1 ) || p.ground != p.ground ) p.ground = spline_groundy( p.x, p.z );
	}
	std::vector<double> prefix( n + 1, 0.0 );
	for ( int i = 0; i < n; i++ ) prefix[ i + 1 ] = prefix[ i ] + c[i].ground;
	const float avgSpacing = c[ n - 1 ].s / (float)(n - 1);
	const int window = avgSpacing > 0 ? (int)(r.smoothing * 0.5f / avgSpacing) : 0;
	for ( int i = 0; i < n; i++ )
	{
		const int a = std::max( 0, i - window ), b = std::min( n - 1, i + window );
		c[i].h = (float)((prefix[ b + 1 ] - prefix[ a ]) / (double)(b - a + 1));
	}

	// pinned to the ground at open ends and at junctions (so it meets an earlier road), the rest shifted to suit
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
	std::sort( pins.begin(), pins.end() );
	pins.erase( std::unique( pins.begin(), pins.end(), []( const std::pair<int, float>& a, const std::pair<int, float>& b ) { return a.first == b.first; } ), pins.end() );
	std::vector<char> pinned( n, 0 );
	if ( !pins.empty() )
	{
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

	// the footprint: every texel near the centre line (one texel steps), its distance and the road height there
	const float reach = halfW + std::max( std::max( r.shoulder, r.grassMargin ), r.treeMargin );
	const int step = std::max( 1, (int)(texel / spacing + 0.5f) );
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

	// the writes: the carriageway flat at the profile, the shoulders blended to the ground (not over an earlier road's
	// carriageway), the textures, the grass cleared
	std::vector<uint32_t> carriageway;
	for ( const auto& entry : foot )
	{
		const uint32_t key = entry.first;
		const sRoadFoot& f = entry.second;
		const int ix = (int)(key % SPLINE_MAP_SIZE), iz = (int)(key / SPLINE_MAP_SIZE);
		const float wx = ((float)ix / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
		const float wz = ((float)iz / SPLINE_MAP_SIZE * 2.0f - 1.0f) * E;
		const uint32_t hIndex = (SPLINE_MAP_SIZE - 1 - iz) * SPLINE_MAP_SIZE + ix;
		const uint32_t mIndex = key;
		const bool bCarriage = f.d <= halfW;
		const bool bProtected = !bCarriage && protectedTexels.count( key ) > 0;
		sSplineBakeTexel b;
		b.x = (uint16_t)ix;
		b.z = (uint16_t)iz;
		b.heightBefore = pH[ hIndex ];
		b.typeBefore = pT[ hIndex ];
		b.matBefore = pM ? pM[ mIndex ] : 0;
		b.grassBefore = pG ? pG[ mIndex ] : 0;
		if ( bCarriage )
		{
			const float e = halfW > 0 ? f.d / halfW : 0;
			pH[ hIndex ] = GGTerrain::GGTerrain_HeightToEdit( f.h - r.crown * e * e );
			pT[ hIndex ] = 1;
			b.flags |= SPLINE_TEXEL_CARRIAGEWAY | SPLINE_TEXEL_HEIGHT;
			carriageway.push_back( key );
		}
		else if ( f.d <= halfW + r.shoulder && !bProtected && r.shoulder > 0 )
		{
			float ground = f.h;
			if ( !GGTerrain::GGTerrain_GetHeight( wx, wz, &ground, 1, 1 ) || ground != ground ) ground = f.h;
			const float tt = (f.d - halfW) / r.shoulder;
			const float smooth = tt * tt * (3.0f - 2.0f * tt);
			pH[ hIndex ] = GGTerrain::GGTerrain_HeightToEdit( f.h + (ground - f.h) * smooth );
			pT[ hIndex ] = 1;
			b.flags |= SPLINE_TEXEL_HEIGHT;
		}
		if ( pM )
		{
			if ( bCarriage && r.material > 0 ) { pM[ mIndex ] = (uint8_t)r.material; b.flags |= SPLINE_TEXEL_MATERIAL; }
			else if ( !bCarriage && !bProtected && r.edgeMaterial > 0 && f.d <= halfW + r.shoulder ) { pM[ mIndex ] = (uint8_t)r.edgeMaterial; b.flags |= SPLINE_TEXEL_MATERIAL; }
		}
		if ( pG && f.d <= halfW + r.grassMargin && !bProtected )
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
	for ( uint32_t key : carriageway ) protectedTexels.insert( key );

	// the trees whose trunks stand within the tree margin of the carriageway
	float minX = FLT_MAX, minZ = FLT_MAX, maxX = -FLT_MAX, maxZ = -FLT_MAX;
	for ( const sRoadSample& p : c ) { minX = std::min( minX, p.x ); minZ = std::min( minZ, p.z ); maxX = std::max( maxX, p.x ); maxZ = std::max( maxZ, p.z ); }
	const float treeReach = halfW + r.treeMargin;
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
		s.bakedSignature = spline_signature( s );
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
		else bBake = !s.baked.empty() || !s.bakedTrees.empty();
		if ( bBake ) { in[ si ] = 1; bAny = true; }
	}
	iApplySpline = -1;
	if ( !bAny ) return;
	spline_joined( in );
	spline_bakegroup( in );
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
		return;
	}

	// the terrain: a new node at the end of the selected spline (or its start, with the first node selected), dragged while held
	float x, y, z;
	if ( !spline_terrainpick( &x, &y, &z ) ) return;
	if ( g_iSplineSelected < 0 ) g_iSplineSelected = spline_new();
	sSpline& s = g_Splines[ g_iSplineSelected ];
	if ( s.closed ) return;
	sSplineNode node;
	node.x = x; node.y = y; node.z = z;
	int at = (int)s.nodes.size();
	if ( g_iSplineNodeSelected == 0 && s.nodes.size() > 1 ) at = 0;
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

// a terrain texture slot (stored + 1), with the Paint tool's current texture a click away
static void spline_rowtexture( const char* pLabel, const char* pId, int* pSlot, float w )
{
	char items[ 33 ][ 16 ];
	const char* pItems[ 33 ];
	strcpy_s( items[ 0 ], 16, "None" );
	pItems[ 0 ] = items[ 0 ];
	for ( int i = 1; i <= 32; i++ ) { sprintf_s( items[ i ], 16, "Texture %d", i ); pItems[ i ] = items[ i ]; }
	spline_row( pLabel );
	ImGui::Combo( pId, pSlot, pItems, 33 );
	char button[ 64 ];
	sprintf_s( button, 64, "Use the Paint Tool's Texture%s", pId );
	ImGui::SetCursorPosX( fRowFieldX );
	if ( ImGui::StyleButton( button, ImVec2( fRowRight - fRowFieldX, 0 ) ) ) *pSlot = iCurrentTextureForPaint + 1;
	if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "The texture selected in Terrain Tools' Paint mode" );
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

	if ( ImGui::StyleCollapsingHeader( "Roads and Rivers", 0 ) )
	{
		ImGui::Indent( 10 );
		if ( ImGui::Checkbox( "Edit Splines##splineeditmode", &g_bSplineEditMode ) )
		{
			bConnectMode = false;
		}
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Click the terrain to add nodes and drag them. Shift+click the curve inserts a node, Ctrl+click deletes one.\nA dragged node snaps to another spline's node or curve and joins it (a T junction or a crossing); Alt+drag pulls it out again.\nAn end dropped on the spline's other end closes it. Alt+drag a Bezier handle to break the pair." );

		if ( ImGui::StyleButton( "New Spline##splinenew", ImVec2( w * 0.45f, 0 ) ) )
		{
			g_iSplineSelected = spline_new();
			g_iSplineNodeSelected = -1;
			g_bSplineEditMode = true;
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
				}
			}
			ImGui::EndChild();
		}

		if ( g_iSplineSelected >= 0 )
		{
			sSpline& s = g_Splines[ g_iSplineSelected ];
			fRowLabelX = ImGui::GetCursorPosX();
			fRowFieldX = fRowLabelX + ImGui::CalcTextSize( "Shoulder Texture" ).x + 12.0f;
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
			spline_row( "Kind" );
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
				bool bAuto = r.autoApply != 0;
				ImGui::SetCursorPosX( fRowLabelX );
				if ( ImGui::Checkbox( "Update the Road as I Edit##splineroadauto", &bAuto ) ) r.autoApply = bAuto ? 1 : 0;
				ImGui::SetCursorPosX( fRowLabelX );
				if ( ImGui::StyleButton( "Apply##splineroadapply", ImVec2( w * 0.45f, 0 ) ) ) iApplySpline = g_iSplineSelected;
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Bake the road into the terrain now" );
				ImGui::SameLine();
				if ( ImGui::StyleButton( "Remove Road##splineroadremove", ImVec2( w * 0.45f, 0 ) ) )
				{
					spline_unbake( g_iSplineSelected );
					s.kind = SPLINE_KIND_NONE;
					s.bakedSignature = spline_signature( s );
					spline_modified();
				}
				if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", "Take the road out of the terrain and keep the spline as a line" );
				if ( s.baked.empty() ) ImGui::TextWrapped( "%s", r.autoApply ? "Not baked yet." : "Not baked: press Apply." );
				else ImGui::Text( "Baked: %d texels, %d trees hidden%s", (int)s.baked.size(), (int)s.bakedTrees.size(), spline_signature( s ) != s.bakedSignature ? " (changed)" : "" );
			}
			else if ( s.kind == SPLINE_KIND_RIVER )
			{
				ImGui::TextWrapped( "%s", "Baking rivers into the terrain comes in the next build." );
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

	if ( g_bSplineEditMode )
	{
		// the terrain's own tools leave the mouse alone meanwhile
		GGTerrain::ggterrain_extra_params.edit_mode = GGTERRAIN_EDIT_NONE;
		GGTerrain::ggterrain_global_render_params2.flags2 &= ~GGTERRAIN_SHADER_FLAG2_SHOW_BRUSH_SIZE;
		spline_mouse();
		spline_draw();
	}
	else
	{
		iDragSpline = iDragNode = -1;
		iDragHandle = 0;
		bConnectMode = false;
	}

	spline_bakechanged();
}

//
// map.spl
//

void spline_deleteall( void )
{
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
}
