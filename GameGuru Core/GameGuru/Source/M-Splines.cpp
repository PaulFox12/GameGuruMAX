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

#include <vector>
#include <algorithm>

extern bool bImGuiGotFocus;
extern bool bImGuiRenderTargetFocus;
extern bool bForceKey2;
extern ImVec2 renderTargetAreaPos;
extern ImVec2 renderTargetAreaSize;
bool Convert3DLineTo2D( float x1, float y1, float z1, float x2, float y2, float z2, ImVec2* pA, ImVec2* pB );
float BT_GetGroundHeight( unsigned long value, float x, float z );

#define SPLINE_CURVE_LINEAR 0
#define SPLINE_CURVE_SMOOTH 1
#define SPLINE_CURVE_BEZIER 2

#define SPLINE_KIND_NONE 0
#define SPLINE_KIND_ROAD 1
#define SPLINE_KIND_RIVER 2

#define SPLINE_NODE_BROKEN 1 // the node's Bezier handles move apart

#define SPLINE_FILE_MAGIC 0x50534747 // 'GGSP'
#define SPLINE_FILE_VERSION 1

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

struct sSpline
{
	int id = 0;
	char name[64] = "";
	int kind = SPLINE_KIND_NONE;
	int curve = SPLINE_CURVE_SMOOTH;
	int closed = 0;
	std::vector<sSplineNode> nodes;
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

static void spline_deletespline( int si )
{
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
			ImGui::PushItemWidth( w * 0.6f );
			if ( ImGui::InputText( "Name##splinename", s.name, 64 ) ) spline_modified();
			const char* curves[] = { "Straight", "Smooth", "Bezier" };
			int curve = s.curve;
			if ( ImGui::Combo( "Curve##splinecurve", &curve, curves, 3 ) && curve != s.curve )
			{
				if ( curve == SPLINE_CURVE_BEZIER ) spline_seedhandles( s, s.curve );
				s.curve = curve;
				spline_modified();
			}
			const char* kinds[] = { "Line only", "Road", "River" };
			if ( ImGui::Combo( "Kind##splinekind", &s.kind, kinds, 3 ) ) spline_modified();
			ImGui::PopItemWidth();
			if ( s.kind != SPLINE_KIND_NONE ) ImGui::TextWrapped( "%s", "Baking roads and rivers into the terrain comes in the next build." );
			bool bClosed = s.closed != 0;
			if ( ImGui::Checkbox( "Closed Loop##splineclosed", &bClosed ) && s.nodes.size() > 2 )
			{
				s.closed = bClosed ? 1 : 0;
				spline_modified();
			}
			ImGui::PushItemWidth( w * 0.6f );
			ImGui::SliderInt( "Pieces##splinepieces", &g_iSplinePieces, 2, 8 );
			ImGui::PopItemWidth();
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
	for ( const sSpline& s : g_Splines )
	{
		// each spline's bytes first, so a reader skips what it doesn't know
		const uint32_t nodeCount = (uint32_t)s.nodes.size();
		const uint32_t bytes = sizeof(int32_t) * 4 + 64 + sizeof(uint32_t) + nodeCount * (sizeof(float) * 7 + sizeof(int32_t) * 2);
		fwrite( &bytes, sizeof(bytes), 1, fp );
		const int32_t values[4] = { s.id, s.kind, s.curve, s.closed };
		fwrite( values, sizeof(values), 1, fp );
		fwrite( s.name, 64, 1, fp );
		fwrite( &nodeCount, sizeof(nodeCount), 1, fp );
		for ( const sSplineNode& node : s.nodes )
		{
			const float f[7] = { node.x, node.y, node.z, node.inX, node.inZ, node.outX, node.outZ };
			const int32_t i[2] = { node.flags, node.junction };
			fwrite( f, sizeof(f), 1, fp );
			fwrite( i, sizeof(i), 1, fp );
		}
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
	for ( uint32_t k = 0; k < header[2]; k++ )
	{
		uint32_t bytes = 0;
		if ( fread( &bytes, sizeof(bytes), 1, fp ) != 1 ) break;
		const long start = ftell( fp );
		sSpline s;
		int32_t values[4] = { 0, 0, 0, 0 };
		uint32_t nodeCount = 0;
		if ( fread( values, sizeof(values), 1, fp ) != 1 || fread( s.name, 64, 1, fp ) != 1 || fread( &nodeCount, sizeof(nodeCount), 1, fp ) != 1 ) break;
		s.name[ 63 ] = 0;
		s.id = values[0]; s.kind = values[1]; s.curve = values[2]; s.closed = values[3];
		if ( nodeCount > 100000 ) break;
		for ( uint32_t n = 0; n < nodeCount; n++ )
		{
			float f[7];
			int32_t i[2];
			if ( fread( f, sizeof(f), 1, fp ) != 1 || fread( i, sizeof(i), 1, fp ) != 1 ) break;
			sSplineNode node;
			node.x = f[0]; node.y = f[1]; node.z = f[2]; node.inX = f[3]; node.inZ = f[4]; node.outX = f[5]; node.outZ = f[6];
			node.flags = i[0]; node.junction = i[1];
			s.nodes.push_back( node );
			if ( node.junction >= g_iSplineNextJunction ) g_iSplineNextJunction = node.junction + 1;
		}
		if ( s.id >= g_iSplineNextID ) g_iSplineNextID = s.id + 1;
		g_Splines.push_back( s );
		fseek( fp, start + (long)bytes, SEEK_SET );
	}
	fclose( fp );
	spline_cleanjunctions();
}
