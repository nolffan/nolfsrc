// server.dll — the physics module (reconstruction). Build /O1.
#include <math.h>
#include "worldsim.h"

// 0x10056ff8 (.bss) -- bspq_TestSphere's node stack, declared in worldsim.h.
// 1000 entries: the next global the image references, g_nPhysMoves at
// 0x10057f98, is exactly 0xfa0 bytes on.
BSPNode* g_NodeStack[1000];

// Walking the tree for anything inside a sphere. The walk keeps its own stack
// rather than recursing: where the sphere straddles a plane, the near side is
// followed at once and the far side is pushed for later — and only if there
// is something below it, so a leaf is never pushed.
//
// A node whose geometry the sphere clears entirely on one side is followed
// down that side alone, which is the whole point of the tree.
//
// The if/else chain is load-bearing. All three arms end by descending the
// front side, and VC6 emits that step once: written as two guarded
// `continue`s with the descend last it hosts the surviving copy at the *end*
// and jumps forward to it, which is three bytes and seven blocks away from
// the shipped layout. Written as if/else, as here, the copy stays at the
// first arm and the other two jump backwards into it -- what shipped.
int bspq_TestSphere(BSPNode* pNode, CSphereQuery* pQuery)
{
	BSPNode** ppTop = g_NodeStack;
	LTVector  v;
	float     fDist;

	for (;;)
	{
		if (pNode->m_Flags & BSPNODE_LEAF)
		{
			if (ppTop == g_NodeStack)
				return 0;

			ppTop--;
			pNode = *ppTop;
		}

		{
			WorldPolyOwner* pOwner = pNode->m_pOwner;
			LTPlane*        pPlane = pOwner->m_pParent->m_pPlane;

			v = pQuery->m_vPoint;

			fDist = v.x * pPlane->m_Normal.x +
			        v.y * pPlane->m_Normal.y +
			        v.z * pPlane->m_Normal.z - pPlane->m_Dist;

			if (fDist > pQuery->m_fRadius)
			{
				pNode = pNode->m_pChild[1];
			}
			else if (fDist < -pQuery->m_fRadius)
			{
				pNode = pNode->m_pChild[0];
			}
			else
			{
				if (pOwner && w_IsPolyBlocking(pOwner))
					return 1;

				{
					BSPNode* pBack = pNode->m_pChild[0];

					if (!(pBack->m_Flags & BSPNODE_LEAF))
					{
						*ppTop = pBack;
						ppTop++;
					}
				}

				pNode = pNode->m_pChild[1];
			}
		}
	}
}

// Hitting a surface. An object already travelling away from the surface is
// not hitting it at all, and nothing is written; otherwise the impulse is the
// surface normal scaled by how fast the object was closing on it, the
// momentum that impulse against the object's mass, and the impulse is then
// nudged back along the normal so that a resting object does not settle
// inside what it is resting on.
//
// This sat as a four-byte near over the dot product's term order for two
// sessions. What closed it is written up at the expression itself.
//
// `inline` is not a hint here, it is the /O2 shape. ws_HandleImpact is the
// only caller in the image, and the shipped client has no standalone copy of
// this function at all: 0x0041a680 (528 bytes) carries
// the dot product, the `fcoms` against zero and the three vector builds in
// its own body. /Ob1 will only expand a callee marked `inline`, and only one
// defined ABOVE its caller, which this is. /O1 declines a 228-byte expansion
// on size, so server.dll keeps both the standalone body at 0x100051ec and the
// call to it.
inline int ws_Bounce(LTObject* pObj, LTVector* pVel, LTVector* pNormal,
                     LTVector* pImpulse, LTVector* pMomentum)
{
	LTVector v = *pVel;
	float    fDot;

	// The grouping is not decoration. VC6 emits an FP add's operands in
	// reverse, so a flat `x*nx + y*ny + z*nz` can only ever put the x term
	// last -- and the shipped code puts the z term last. Only a sum whose
	// right operand is itself an add reaches this order; all twelve flat and
	// parenthesised spellings were compiled to check, under all four held
	// compiler revisions.
	fDot = v.z * pNormal->z + (v.x * pNormal->x + v.y * pNormal->y);

	if (fDot < 0.0f)
	{
		fDot = -fDot;

		*pImpulse = *pNormal * fDot;

		*pMomentum = (*pImpulse * WS_IMPULSE) * pObj->m_Mass;

		*pImpulse += *pNormal * WS_SEPARATION;

		return 1;
	}

	return 0;
}

// One coordinate against one axis of the region. The two bounds are tested
// in the order they are stored, so a coordinate below the minimum is
// reported without the maximum being looked at.
//
// The `else if`/`else` is what makes the three constant returns land as they
// shipped: each writes out its own `pop eax; ret` rather than cross-jumping
// onto the next one's. Three separate `if (...) return X;` statements share
// one exit instead.
int CSimContext::ClassifyAxis(float f)
{
	if (f < m_fMin)
		return WS_BELOW;
	else if (f > m_fMax)
		return WS_ABOVE;
	else
		return WS_INSIDE;
}

float g_fStepMinNormalY = 0.1f;
float g_fStepSlopeLimit = 0.7071f;

CSphereQuery g_BoxSphere;
CSphereQuery g_SphereAtOffset;
CSphereQuery g_SphereAtDelta;
float        g_fBoxRadius;

LTVector g_SweepPoints[2][8];
// The box's own six faces as planes. Only the distances ever change --
// ws_SetupBoxGeometry touches nothing but m_Dist -- so the six axis normals
// have to be written once, and the initialiser below is where this
// reconstruction does it.
//
// The shipped source did NOT: the array is at 0x10057fc8 (read off
// ws_SetupBoxGeometry's own six stores, which land on +0xc, +0x1c, ... +0x5c),
// and .data's raw data stops at 0x10057000 — so g_BoxPlanes is in the
// uninitialised tail and its normals are set somewhere that has not been
// found yet -- some world- or physics-init the reconstruction has not reached
// -- and not at file scope. Keeping the initialiser here costs nothing for
// byte-matching, because every function that reads the array reads it through
// a masked relocation and only the section the array lands in differs, and it
// keeps the array correct when the game runs. Move it to whatever
// initialiser turns up rather than deleting it.
LTPlane  g_BoxPlanes[6] = {
	LTPlane( 1.0f,  0.0f,  0.0f, 0.0f),
	LTPlane(-1.0f,  0.0f,  0.0f, 0.0f),
	LTPlane( 0.0f,  1.0f,  0.0f, 0.0f),
	LTPlane( 0.0f, -1.0f,  0.0f, 0.0f),
	LTPlane( 0.0f,  0.0f,  1.0f, 0.0f),
	LTPlane( 0.0f,  0.0f, -1.0f, 0.0f)
};

// Turning a box into something the tree walk can test. Only the two faces
// square to Y are built: the walk needs a wound face to clip against and the
// four sides fall out of the six planes, whose distances are the box's own
// corners. Each face is kept twice, so that the copy can be clipped while the
// original stays as it was.
//
// `inline`, and both of the two things that follow from it are in the shipped
// client. ws_GetBoxStatus (0x00417840, 968 bytes against our 432) writes the
// whole of this body into itself -- the run of stores into g_SweepPoints at
// 0x4b2830 is right there ahead of its bspq_TestSphere call -- while
// ws_SetupSweptBox still CALLS it at 0x004183f5, which is what keeps the
// standalone copy at 0x00418410 in the image and matching. That is VC6
// deciding per call site, so the keyword does not cost the out-of-line body.
// /O1 declines a 562-byte expansion outright, so server.dll is untouched.
inline void ws_SetupBoxGeometry(LTVector* pMin, LTVector* pMax)
{
	g_SweepPoints[0][0].Init(pMin->x, pMax->y, pMin->z);
	g_SweepPoints[0][1].Init(pMin->x, pMax->y, pMax->z);
	g_SweepPoints[0][2].Init(pMax->x, pMax->y, pMax->z);
	g_SweepPoints[0][3].Init(pMax->x, pMax->y, pMin->z);

	g_SweepPoints[0][4] = g_SweepPoints[0][0];
	g_SweepPoints[0][5] = g_SweepPoints[0][1];
	g_SweepPoints[0][6] = g_SweepPoints[0][2];
	g_SweepPoints[0][7] = g_SweepPoints[0][3];

	g_SweepPoints[1][0].Init(pMin->x, pMin->y, pMin->z);
	g_SweepPoints[1][1].Init(pMin->x, pMin->y, pMax->z);
	g_SweepPoints[1][2].Init(pMax->x, pMin->y, pMax->z);
	g_SweepPoints[1][3].Init(pMax->x, pMin->y, pMin->z);

	g_SweepPoints[1][4] = g_SweepPoints[1][0];
	g_SweepPoints[1][5] = g_SweepPoints[1][1];
	g_SweepPoints[1][6] = g_SweepPoints[1][2];
	g_SweepPoints[1][7] = g_SweepPoints[1][3];

	g_BoxPlanes[0].m_Dist =  pMin->x;
	g_BoxPlanes[1].m_Dist = -pMax->x;
	g_BoxPlanes[2].m_Dist =  pMin->y;
	g_BoxPlanes[3].m_Dist = -pMax->y;
	g_BoxPlanes[4].m_Dist =  pMin->z;
	g_BoxPlanes[5].m_Dist = -pMax->z;
}

// Will the physics collide with this polygon? Three answers, cheapest first.
// Geometry the world data does not mark solid is not collided with at all;
// geometry whose bounding sphere is further from the box's bounding sphere
// than the two radii together cannot be reached; and everything else is
// settled by clipping the polygon against the box's own six planes and asking
// whether anything survived.
//
// The clip is Sutherland-Hodgman, run six times with the two vertex lists
// swapping roles each round. Only the input list is the polygon's own vertex
// pointers; every list after that is built in the working array, and every
// vertex the clip invents is bump-allocated out of vClipped, which is never
// reset -- so the two arrays have to be big enough for all six rounds at once
// rather than for one.
//
// The clip planes are read one number at a time: an axis-aligned normal is
// the plane's index halved for the axis and g_BoxSigns[i] for the direction,
// so the distance is one multiply rather than a dot product.
//
// Byte-identical on server.dll (0x10001cad). The last instruction was the
// zeroing of nStored: shipped has `and %esi,-0x1c(%ebp)` (the /O1
// `and [x],0` idiom with the zero already in %esi substituted for the
// immediate) ahead of the first `lea`, where a `nStored = 0` written before
// the three list-pointer assignments gave `mov %esi` one instruction later.
// The statement order is pointers, then the vertex count, then nStored. It
// also helps lithtech.exe (0x4152f0, 621 bytes): that build reads m_nVertices
// after the pointer `lea`s, and all that is left there is the order the /O2
// build squares the sphere-reject terms in (z, x, y where ours does x, y, z).
// Changing the term order in the source does not move it on either image.
//
// Two earlier fixes are worth remembering. The four terms
// of the sphere reject are named locals assigned in the order sum, z, y, x --
// the reverse of the order the comparison uses them, which is the order a
// stack machine has to push them in. And `pDist` is a real local: naming
// `&g_BoxPlanes[i].m_Dist` once per round is what puts the plane's induction
// variable in the first frame slot and makes it, rather than g_BoxSigns', the
// one the loop's exit test compares. Writing `g_BoxPlanes[i].m_Dist` at both
// use sites gives the same instructions with two frame slots transposed and
// the exit test on the other induction variable.
int w_IsPolyBlocking(WorldPolyOwner* pOwner)
{
	LTVector   vClipped[WS_MAX_CLIP_VERTS];
	LTVector*  pWork[WS_MAX_CLIP_VERTS];
	LTVector*  pInList[WS_MAX_POLY_VERTS];
	LTVector** ppIn;
	LTVector** ppOut;
	LTVector** ppOutBase;
	float      fPrevDist;
	float      fCurDist;
	float      fSign;
	int        nStored;
	int        bPrevIn;
	int        bCurIn;
	int        nAxis;
	int        nPrev;
	int        nVerts;
	int        i;
	int        j;
	float*     pDist;
	float      fSum;
	float      fDX, fDY, fDZ;

	g_nPhysStatA8++;

#ifdef LT_SERVER_TIMERS
	CountAdder cntAdder(&g_nPhysStatA4);
#endif

	if (!(pOwner->m_pParent->m_nFlags & WBSP_SOLID))
		return 0;

	// All four terms are pushed onto the FPU stack before any of the squaring
	// happens, deepest first, which is the reverse of the order they are used
	// in: so the four assignments below run sum, z, y, x and the comparison
	// then reads them back x, y, z, sum.
	fSum = g_BoxSphere.m_fRadius + pOwner->m_fRadius;
	fDZ  = g_BoxSphere.m_vPoint.z - pOwner->m_vCentre.z;
	fDY  = g_BoxSphere.m_vPoint.y - pOwner->m_vCentre.y;
	fDX  = g_BoxSphere.m_vPoint.x - pOwner->m_vCentre.x;

	if (fDX * fDX + fDY * fDY + fDZ * fDZ > fSum * fSum)
		return 0;

	ppOut     = pWork;
	ppOutBase = pWork;
	ppIn      = pInList;

	nVerts = pOwner->m_nVertices;

	nStored = 0;

#ifdef NOLFSRC_MODERN
	// fixed: w_IsPolyBlocking has no bounds checks (RETAIL_BUGS.md). A
	// polygon too big for the lists, or a clip that would run out of room,
	// is answered as blocking: it has already passed the sphere test, and
	// colliding with it is the safe mistake.
	if (nVerts > WS_MAX_POLY_VERTS)
		return 1;

#endif
	for (j = 0; j < nVerts; j++)
		pInList[j] = pOwner->m_Vertices[j].m_pPos;

	for (i = 0; i < 6; i++)
	{
		pDist = &g_BoxPlanes[i].m_Dist;
		fSign = g_BoxSigns[i];
		nAxis = i >> 1;

		nPrev = nVerts - 1;

		fPrevDist = fSign * (*ppIn[nPrev])[nAxis] - *pDist;

		if (fPrevDist > WS_CLIP_SLOP)
			bPrevIn = 1;
		else
			bPrevIn = 0;

		for (j = 0; j < nVerts; j++)
		{
#ifdef NOLFSRC_MODERN
			// Room for this step's two pointers and one new vertex.
			if (ppOut + 2 > pWork + WS_MAX_CLIP_VERTS ||
			    nStored >= WS_MAX_CLIP_VERTS)
				return 1;

#endif
			fCurDist = fSign * (*ppIn[j])[nAxis] - *pDist;

			if (fCurDist > WS_CLIP_SLOP)
				bCurIn = 1;
			else
				bCurIn = 0;

			if (bPrevIn)
			{
				*ppOut = ppIn[nPrev];
				ppOut++;
			}

			if (bPrevIn != bCurIn)
			{
				// Both ends are written out long, not through locals: the
				// shipped code re-reads ppIn[nPrev] once per component (two
				// uses each) and keeps only the *address* of ppIn[j], which
				// it already has from the distance above.
				float fT = fPrevDist / (fPrevDist - fCurDist);

				vClipped[nStored].x = (ppIn[j]->x - ppIn[nPrev]->x) * fT +
				                      ppIn[nPrev]->x;
				vClipped[nStored].y = (ppIn[j]->y - ppIn[nPrev]->y) * fT +
				                      ppIn[nPrev]->y;
				vClipped[nStored].z = (ppIn[j]->z - ppIn[nPrev]->z) * fT +
				                      ppIn[nPrev]->z;

				*ppOut = &vClipped[nStored];
				ppOut++;
				nStored++;
			}

			fPrevDist = fCurDist;
			nPrev     = j;
			bPrevIn   = bCurIn;
		}

		// The count is taken before the base moves on: that keeps ppOut live
		// across the subtraction, so the shipped code copies it into a second
		// register to subtract in and stores the original. Assigning the base
		// first instead lets the subtraction happen in place and comes out
		// two bytes shorter.
		ppIn      = ppOutBase;
		nVerts    = ppOut - ppIn;
		ppOutBase = ppOut;

		if (nVerts == 0)
			return 0;
	}

	return 1;
}

// Everything the walk needs, from where the sweep starts and ends and how big
// the thing being swept is. Squares are kept alongside their roots because
// almost every test the walk makes compares squares — the roots are only
// wanted where a length has to be halved or divided by.
//
// A sweep that does not move has no direction, and gets a zero one rather
// than a division: the tests that use it are all against the midpoint, which
// is still meaningful.
//
// Three things closed it, on both images (server 0x10002637, lithtech
// 0x4169e0):
//   * the zero direction is `m_vDir.Init()`, not three component stores. The
//     inlined call makes VC6 forget which FPU registers held which members,
//     so the shipped build drops m_fLength at the join and re-reads it for
//     m_fReach (`flds 0x4c(%ebx)`), and at /O2 re-reads m_fCornerDist too
//     (`fadds 0x30(%ecx)`). With component stores ours carried both on the
//     FPU stack across the join, which also cost the midpoint's x sum its
//     register.
//   * the reciprocal is written inline, `m_vDelta * (1.0f / m_fLength)`: the
//     shipped `fdiv %st(1),%st; fstp %st(1)` keeps the length alive for the
//     divide and drops it after; a named fInv gave `fdivp`.
//   * m_fHitDepth is cleared before m_pHitPoly: /O2 keeps that store order
//     (0x98 before 0x9c at 0x416b80); /O1 emits the same bytes either way.
//
// The two extents closed on the SDK macros. Two things were wrong before:
// the arms were the wrong way round (the shipped `jae`/`jbe` jump to the
// m_vEnd arm and fall through to m_vStart, which is the false-branch polarity
// of `m_vStart > m_vEnd`, not of `m_vEnd >= m_vStart`), and the operands were
// named in locals, where the shipped code reads the members four times per
// statement. Writing them as members with the condition inverted got the
// polarity but still compiled the compare as one load and an `fcomps` against
// memory. LTMAX/LTMIN evaluate `(a)` twice, and that is what makes VC6 push
// *both* operands and compare with `fcompp` — the shipped shape exactly.
void CSimContext::Setup()
{
	m_fRadiusSqr = m_fRadius * m_fRadius;
	m_fCornerSqr = m_fHeight * m_fHeight + m_fRadiusSqr;
	m_fCornerDist = (float)sqrt(m_fCornerSqr);

	m_vDelta = m_vEnd - m_vStart;

	m_fLengthSqr = m_vDelta.Dot(m_vDelta);
	m_fLength = (float)sqrt(m_fLengthSqr);

	if (m_fLength != 0.0f)
	{
		m_vDir = m_vDelta * (1.0f / m_fLength);
	}
	else
	{
		m_vDir.Init();
	}

	m_vMid = (m_vStart + m_vEnd) * 0.5f;

	m_fReach = m_fLength * 0.5f + m_fCornerDist;
	m_fReachSqr = m_fReach * m_fReach;

	// Both extents read the members, not locals: each comparison pushes
	// m_vStart.y and m_vEnd.y afresh, compares them with `fcompp`, and then
	// each arm loads its own member again. Four loads per statement where a
	// pair of locals would have made two.
	m_fMax = LTMAX(m_vStart.y, m_vEnd.y) + m_fHeight;
	m_fMin = LTMIN(m_vStart.y, m_vEnd.y) - m_fHeight;

	m_fHitDepth = 0.0f;
	m_pHitPoly = NULL;
}

// Which side of a plane the swept volume falls on. The volume is a cylinder,
// so how far it reaches towards a plane depends on how the plane is tilted:
// a plane whose normal is level is cleared by the radius alone, one whose
// normal is straight up or down by the height alone, and anything between by
// a blend of the two. The cheap flag asks for the corner distance instead —
// the largest either could be — which is right whenever a wrong answer only
// costs more work later.
// Every exit is written as one explicit if/else chain whose arms each end in
// their own return: that is what puts the merged `return WS_BEHIND` INLINE at
// its first site as shipped (early guarded returns host it at the last; a goto
// to the first site does not move it). The overlap arms are `if (bWantOverlap)
// m_fOverlap = ...; return WS_STRADDLE;` and the final BEHIND arm keeps its
// single return -- splitting that into two returns cost lithtech.exe eight
// bytes.
//
// The dot product is written as an accumulator for the reason set out at
// ws_ProjectBox: the shipped build emits its terms in source order, which a
// single `a + b + c` expression cannot do.
int CSimContext::ClassifyPlane(LTPlane* pPlane, LTVector* pPoint,
                               int bWantOverlap, int bCheap)
{
	LTVector v = *pPoint;
	float    fDist;
	float    fReach;
	float    fFlat;

	fDist  = v.x * pPlane->m_Normal.x;
	fDist += v.y * pPlane->m_Normal.y;
	fDist += v.z * pPlane->m_Normal.z;
	fDist -= pPlane->m_Dist;

	if (bWantOverlap)
		m_fPlaneDist = fDist;

	if (bCheap)
	{
		if (fDist >= m_fCornerDist)
		{
			return WS_INFRONT;
		}
		else if (-m_fCornerDist >= fDist)
		{
			return WS_BEHIND;
		}
		else if (pPlane->m_Normal.y == 0.0)
		{
			if (fDist >= m_fRadius)
			{
				return WS_INFRONT;
			}
			else if (-m_fRadius >= fDist)
			{
				return WS_BEHIND;
			}
			else
			{
				if (bWantOverlap)
					m_fOverlap = m_fRadius - fDist;

				return WS_STRADDLE;
			}
		}
		else if (pPlane->m_Normal.x == 0.0 && pPlane->m_Normal.z == 0.0)
		{
			if (fDist >= m_fHeight)
			{
				return WS_INFRONT;
			}
			else if (-m_fHeight >= fDist)
			{
				return WS_BEHIND;
			}
			else
			{
				if (bWantOverlap)
					m_fOverlap = m_fHeight - fDist;

				return WS_STRADDLE;
			}
		}
	}

	// One expression, so the two cached normal components stay on the stack
	// until sqrt's argument is stored (0x100028a4: `fstpl (%esp)` before the
	// two `fstp %st(0)`). The (double) on the x square keeps it first; without
	// it VC6 squares z first and lithtech.exe breaks (512 bytes). As two
	// statements the stack is tidied before the store instead.
	fFlat = (float)sqrt((double)(pPlane->m_Normal.x * pPlane->m_Normal.x) +
	                    pPlane->m_Normal.z * pPlane->m_Normal.z);

	fReach = fFlat * m_fRadius;
	fReach = (float)sqrt(1.0f - fFlat * fFlat) * m_fHeight + fReach;

	if (fDist >= fReach)
	{
		return WS_INFRONT;
	}
	else if (-fReach >= fDist)
	{
		if (!bCheap && bWantOverlap)
			m_fOverlap = (fReach + fReach) - fDist;

		return WS_BEHIND;
	}
	else
	{
		if (bWantOverlap)
			m_fOverlap = fReach - fDist;

		return WS_STRADDLE;
	}
}

// The move in progress. The from-point and the wanted delta are what the
// caller asked for; the offset is what the solver has had to add to keep the
// box out of the world, and every contact found along the way is remembered
// in the list below so the same object cannot push twice.
int       g_nContacts;
WSContact g_Contacts[WS_MAX_CONTACTS];

LTVector    g_vMoveBase;
LTVector    g_vMoveStart;
LTVector    g_vMoveEnd;
CMoveState*    g_pMoveState;
CBounceState*  g_pMoveResult;
CPhysicsBlock* g_pPBlock;

// Statistics, all zeroed at the top of each move and read back by the timing
// console commands (names ours).
uint32 g_nPhysMoves;
uint32 g_nPhysNodes;
uint32 g_nPhysContactTime;
uint32 g_nPhysStatA4;
uint32 g_nPhysStatA8;
uint32 g_nPhysStepTime;
uint32 g_nPhysMoveTime;

// Nothing is this far away, so the first corner always wins the comparison.
#define WS_FAR 1.0e37f

// Byte-identical on both images. `vTest.Init(...)` rather than three
// component stores is what lets the prologue's pushes interleave with the FP
// adds as shipped; `vPush = normal * (BIAS - fMin)` through the operator is
// what keeps fMin on the stack (`flds BIAS; fsub %st(1),%st; fstp %st(1)`)
// instead of a named fPush's `fsubrs`; and with Init in place,
// `for (i = 0, nTries = 0; ...)` orders lithtech's two zeroing xors. The
// shipped code loads g_vMoveBase first and adds g_vMoveEnd; VC6 loads the
// right operand of the sum first, so the source names g_vMoveEnd first (the
// data-reference check of ext/tools 9c3083b found the operands swapped).
//
// Push the box out of every contact plane it is still inside. One pass is not
// enough: pushing clear of one plane can bury the box in another, so a plane
// that had to push restarts the sweep. Ten pushes is the budget — a box in a
// corner tight enough to need more is stuck, and no amount of shoving will
// free it.
void ws_PushBoxOutOfContacts()
{
	CMoveState* pState;
	WSContact*  pContact;
	LTVector*   pCorner;
	LTVector    vCorners[8];
	LTVector    vTest;
	LTVector    vPush;
	float       fMin, fDist;
	int         i, nTries, n;

	vTest.Init(g_vMoveEnd.x + g_vMoveBase.x,
	           g_vMoveEnd.y + g_vMoveBase.y,
	           g_vMoveEnd.z + g_vMoveBase.z);

	pState = g_pMoveState;

	vCorners[0].x = vTest.x + pState->m_vDims.x;
	vCorners[0].y = vTest.y + pState->m_vDims.y;
	vCorners[0].z = vTest.z + pState->m_vDims.z;
	vCorners[1].x = vTest.x - pState->m_vDims.x;
	vCorners[1].y = vTest.y + pState->m_vDims.y;
	vCorners[1].z = vTest.z + pState->m_vDims.z;
	vCorners[2].x = vTest.x - pState->m_vDims.x;
	vCorners[2].y = vTest.y + pState->m_vDims.y;
	vCorners[2].z = vTest.z - pState->m_vDims.z;
	vCorners[3].x = vTest.x + pState->m_vDims.x;
	vCorners[3].y = vTest.y + pState->m_vDims.y;
	vCorners[3].z = vTest.z - pState->m_vDims.z;
	vCorners[4].x = vTest.x + pState->m_vDims.x;
	vCorners[4].y = vTest.y - pState->m_vDims.y;
	vCorners[4].z = vTest.z + pState->m_vDims.z;
	vCorners[5].x = vTest.x - pState->m_vDims.x;
	vCorners[5].y = vTest.y - pState->m_vDims.y;
	vCorners[5].z = vTest.z + pState->m_vDims.z;
	vCorners[6].x = vTest.x - pState->m_vDims.x;
	vCorners[6].y = vTest.y - pState->m_vDims.y;
	vCorners[6].z = vTest.z - pState->m_vDims.z;
	vCorners[7].x = vTest.x + pState->m_vDims.x;
	vCorners[7].y = vTest.y - pState->m_vDims.y;
	vCorners[7].z = vTest.z - pState->m_vDims.z;

	for (i = 0, nTries = 0; i < g_nContacts; i++)
	{
		if (nTries >= WS_PUSH_TRIES)
			break;

		pContact = &g_Contacts[i];

		fMin = WS_FAR;

		pCorner = vCorners;
		n = 8;

		do
		{
			LTVector v = *pCorner;

			fDist  = v.x * pContact->m_Plane.m_Normal.x;
			fDist += v.y * pContact->m_Plane.m_Normal.y;
			fDist += v.z * pContact->m_Plane.m_Normal.z;
			fDist -= pContact->m_Plane.m_Dist;

			if (fDist < fMin)
				fMin = fDist;

			pCorner++;
			n--;
		}
		while (n);

		if (fMin < WS_PUSH_SLOP)
		{
			vPush = pContact->m_Plane.m_Normal * (WS_PUSH_BIAS - fMin);

			g_vMoveEnd.x += vPush.x;
			g_vMoveEnd.y += vPush.y;
			g_vMoveEnd.z += vPush.z;

			pCorner = vCorners;
			n = 8;

			do
			{
				pCorner->x += vPush.x;
				pCorner->y += vPush.y;
				pCorner->z += vPush.z;

				pCorner++;
				n--;
			}
			while (n);

			i = -1;
			nTries++;
		}
	}
}

// Remember a plane the box has hit and shove the box out along its normal.
// One contact is easy — the shove is the whole answer — but the moment there
// are two the pushes fight each other, so the solver has to be run.
void ws_AddContact(LTPlane* pPlane, void* pObject, float fDepth)
{
#ifdef LT_SERVER_TIMERS
	CountAdder cntAdder(&g_nPhysContactTime);
#endif
	LTVector   vPush;
	float      fPush;
	int        i;

	fPush = fDepth + WS_PUSH_BIAS;

	vPush.x = fPush * pPlane->m_Normal.x;
	vPush.y = fPush * pPlane->m_Normal.y;
	vPush.z = fPush * pPlane->m_Normal.z;

	g_vMoveEnd.x += vPush.x;
	g_vMoveEnd.y += vPush.y;
	g_vMoveEnd.z += vPush.z;

	if (g_nContacts >= WS_MAX_CONTACTS)
		return;

	if (pObject)
	{
		for (i = 0; i < g_nContacts; i++)
		{
			if (g_Contacts[i].m_pObject == pObject)
				return;
		}
	}

	g_Contacts[g_nContacts].m_Plane = *pPlane;
	g_Contacts[g_nContacts].m_pObject = pObject;
	g_nContacts++;

	if (g_nContacts > 1)
		ws_PushBoxOutOfContacts();
}

// Is the swept box clear of this polygon? Every edge of the polygon carries a
// plane that stands on it at right angles to the polygon's own, and the box
// misses the polygon if all sixteen of its points fall outside any one of
// them. The two arrays are the box where the move starts and where it ends.
// Both loops test at the bottom and return from inside: the shipped code ends
// each in `cmp; jae <return>; jmp top` (0x100046da, 0x100046f2; lithtech
// 0x4198d0, 0x4198ed) with `return 1` laid out before `return 0`. A for loop's
// own condition cannot give that -- `for (j...)` is not rotated at all, and
// `for (;;)` or do/while peel the first test (378 bytes). pCur, the edge as a
// vector difference, and the unnamed DistTo/Dot arguments set the register
// choice; lithtech 0x419700 is byte-identical.
// NEAR (server, 340 of 340 bytes, 4 instructions): the m_Dist dot product
// multiplies z, x, y at 0x10004649; ours does y, x, z. Operand order only.
int ws_IsBoxOutsidePoly(WorldPolyOwner* pPoly, LTVector* pPoints1,
                        LTVector* pPoints2)
{
	LTVector *pCur;
	uint32   j;
	uint32   i;
	LTVector vEdge;
	LTPlane  edgePlane;
	uint32   nPrev;

	nPrev = pPoly->m_nVertices - 1;

	for (i = 0; i < pPoly->m_nVertices; )
	{
		pCur = pPoly->m_Vertices[i].m_pPos;
		vEdge = *pCur - *pPoly->m_Vertices[nPrev].m_pPos;

		edgePlane.m_Normal = pPoly->GetPlane()->m_Normal.Cross(vEdge);
		edgePlane.m_Normal.Norm();

		edgePlane.m_Dist = edgePlane.m_Normal.Dot(*pPoly->m_Vertices[i].m_pPos);

		j = 0;
		while (1)
		{
			if (edgePlane.DistTo(pPoints1[j]) < 0.0f)
				goto NextEdge;
			if (edgePlane.DistTo(pPoints2[j]) < 0.0f)
				goto NextEdge;
			j++;
			if (j >= 8)
				return 1;
		}

NextEdge:
		nPrev = i;
		i++;
		if (i >= pPoly->m_nVertices)
			return 0;
	}
	return 0;
}

// One surface has been hit: describe it for whoever asked, work out what the
// hit does to the object, and add that to the running total. The plane only
// comes from the polygon when the thing hit was the world -- an object has no
// polygon to name, and its own blocking plane has already been filled in.
//
// lithtech.exe (0x0041a680, /O2) expands ws_Bounce here but runs out of
// inline budget inside it: the three `_CVector(x,y,z)` constructors under the
// first three `operator*`s, the fourth `operator*` and ws_Bounce's `+=` are
// calls (0x411d40, 0x41ab70, 0x41abc0). The two `+=` in the tail are what
// spend that budget -- written as component adds, nothing is refused and the
// function came out 308/469 bytes with no calls. As written it is 546 against
// 528: everything matches except that ws_Bounce's `+=` is still expanded.
// Tried without closing it (all measured): the tail as any mix of component
// adds, `+=` and `a = a + b`, through pointer locals or not; vTotal through
// the pAdd local, direct, as components or `= ; +=`; the nested
// m_pOwner->GetPlane(); the m_hPoly ternary; ws_Bounce without its `v` copy.
// The refusal set is not monotone in the number of expansions, so this is
// left here.
void ws_HandleImpact(CollisionInfo* pInfo, CBounceState* pState,
                     LTObject* pObj, HOBJECT hObject, IWorldBsp* pWorldBsp,
                     WorldPoly* pPoly, int bWorld)
{
	LTVector vTotal;
	LTVector vImpulse;
	LTVector vMomentum;

	if (bWorld)
		pInfo->m_Plane = *pPoly->GetPlane();

	pInfo->m_hObject = hObject;

	if (pWorldBsp)
		pInfo->m_hPoly = pWorldBsp->MakeHPoly(pPoly);
	else
		pInfo->m_hPoly = INVALID_HPOLY;

	vTotal = pObj->m_Velocity + pState->m_vVelocityAdd;

	if (ws_Bounce(pObj, &vTotal, &pInfo->m_Plane.m_Normal,
	              &vImpulse, &vMomentum))
	{
		if (pObj->m_Flags & FLAG_TOUCH_NOTIFY)
		{
			pState->m_vMomentum += vMomentum;
		}

		pObj->m_ChangeFlags |= CHANGEFLAG_MOTION;

		pState->m_vVelocityAdd += vImpulse;
	}
}

// The other way to answer a contact: instead of shoving the box out along the
// plane's normal, wind the move back to the fraction of it that reaches the
// plane and no further. That is what a slide wants -- the box keeps the part
// of its travel that was legal and loses the part that was not.
// NEAR, and paired again: server 259/259 (same 89 instructions, 16 differ at
// byte level), lithtech 308 of 312. The delta is ONE `vDelta.Init(...)` call,
// not three component stores: through Init's by-value arguments VC6 no longer
// CSEs g_vMoveStart.x from the delta into the `+=` tail, so the tail reloads it
// (`fadds 0x10058290` at 0x1000452d) as shipped does. The component spelling
// kept it in st(3) for the whole function and came out 255 bytes with a 12-byte
// prefix, which the pairing could not place. What Init costs: VC6 now promotes
// vDelta and shares vScaled's slot, so the frame is 0xc against the shipped
// 0x18 (0x100044b7), and lithtech, which was 312/312 with four operand-order
// residuals in the scale, drops to 308. Tried and no better on server.dll:
// `vDelta = g_vMoveEnd - g_vMoveStart` (copies g_vMoveStart, 261), vDelta's
// ctor (= Init), `vDelta -= g_vMoveStart` (262), VEC_SUB, a pointer to
// g_vMoveStart or g_vMoveEnd, VEC_DOT or vDelta.Dot, vScaled.Init / LTVector(...) /
// VEC_MULSCALAR for the scale, `g_vMoveEnd = vDelta * fRatio`, and VEC_ADD in
// the tail. Declaring g_vMoveBase/Start/End as one struct kills the CSE with
// the component spelling too (vDelta at -0x18 is kept), but it costs
// ws_MoveObject its match, so the globals really are separate.
void ws_AddClippedContact(LTPlane* pPlane, void* pObject, float fDist)
{
	LTVector vDelta;
	LTVector vScaled;
	float    fDot, fNew, fRatio;
	int      i;

	vDelta.Init(g_vMoveEnd.x - g_vMoveStart.x,
	            g_vMoveEnd.y - g_vMoveStart.y,
	            g_vMoveEnd.z - g_vMoveStart.z);

	fDot = pPlane->m_Normal.Dot(vDelta);

	fNew = fDist - fDot;

#ifdef NOLFSRC_MODERN
	// fixed: ws_AddClippedContact divides by a dot product that can be zero
	// (RETAIL_BUGS.md). The divisor, fDist - fNew, is fDot: a move parallel
	// to the plane never reaches it, so the move is left as it is.
	if (fDot != 0.0f)
	{
		fRatio = -(fNew / (fDist - fNew));

		vScaled = vDelta * fRatio;

		g_vMoveEnd = vScaled;

		g_vMoveEnd.x += g_vMoveStart.x;
		g_vMoveEnd.y += g_vMoveStart.y;
		g_vMoveEnd.z += g_vMoveStart.z;
	}
#else
	fRatio = -(fNew / (fDist - fNew));

	vScaled = vDelta * fRatio;

	g_vMoveEnd = vScaled;

	g_vMoveEnd.x += g_vMoveStart.x;
	g_vMoveEnd.y += g_vMoveStart.y;
	g_vMoveEnd.z += g_vMoveStart.z;
#endif

	if (g_nContacts >= WS_MAX_CONTACTS)
		return;

	if (pObject)
	{
		for (i = 0; i < g_nContacts; i++)
		{
			if (g_Contacts[i].m_pObject == pObject)
				return;
		}
	}

	g_Contacts[g_nContacts].m_Plane = *pPlane;
	g_Contacts[g_nContacts].m_pObject = pObject;
	g_nContacts++;

	if (g_nContacts > 1)
		ws_PushBoxOutOfContacts();
}

// Project all eight corners of the swept box onto a plane and keep the
// nearest and furthest. Answering which side the box is on then costs two
// comparisons, and the interval it leaves behind is what tells a caller how
// deeply the box has crossed.
//
// **The rule that closed this one, and three others in this file.** VC6
// emits an FP add's two operands in REVERSE order, so a single-expression
// sum `a + b + c` comes out c, b, a -- and no permutation of the source
// terms reaches every emission order, because the reversal is applied to
// whatever order you write. An ACCUMULATOR does not reverse: written as
//
//     f  = a;
//     f += b;
//     f += c;
//
// the terms come out a, b, c, in source order, and the value never leaves
// the FPU stack, so the two forms are the same instructions in a different
// order and exactly the same size. The shipped build uses the flat sum in
// ws_ProjectBoxCheap (which emits z, y, x, and matched with `x + y + z`
// written out) and the accumulator here, three lines away. Both spellings
// are in this file for that reason, and neither is a stylistic choice.
int ws_ProjectBox(CBoxProject* pProject)
{
	LTVector* pPoint = pProject->m_pPoints;
	LTPlane*  pPlane = pProject->m_pPlane;
	float     fDist;
	int       n;

	pProject->m_fMin =  WS_FAR;
	pProject->m_fMax = -WS_FAR;

	n = 8;

	do
	{
		LTVector v = *pPoint;

		fDist  = v.x * pPlane->m_Normal.x;
		fDist += v.y * pPlane->m_Normal.y;
		fDist += v.z * pPlane->m_Normal.z;
		fDist -= pPlane->m_Dist;

		if (fDist < pProject->m_fMin)
			pProject->m_fMin = fDist;

		if (fDist > pProject->m_fMax)
			pProject->m_fMax = fDist;

		pPoint++;
		n--;
	}
	while (n);

	// Member first, constant second. /O1 common-subexpressions the constant
	// into st(0) and reaches the same bytes either way -- which is why this
	// matched in server.dll spelled the other way round -- but /O2 reloads
	// per comparison and then honours source order: the shipped client emits
	// `flds 0xc(%ecx); fcomps <slop>` three times, so the member is the left
	// operand. Spelling only; the three predicates are unchanged.
	if (pProject->m_fMin < WS_PLANE_SLOP && pProject->m_fMax > WS_PLANE_SLOP)
		return WS_STRADDLE;
	else if (pProject->m_fMin > WS_PLANE_SLOP)
		return WS_INFRONT;
	else
		return WS_BEHIND;
}

// The same answer for a plane the box is nowhere near, without touching the
// corners: the box's bounding sphere clears the plane by its radius or it
// does not, and only the undecided case pays for the eight projections.
int ws_ProjectBoxCheap(CBoxProject* pProject)
{
	CSphereQuery* pSphere;
	LTPlane*      pPlane;
	float         fDist;

	g_nPhysMoves++;

	pSphere = pProject->m_pSphere;
	pPlane  = pProject->m_pPlane;

	LTVector v = pSphere->m_vPoint;

	fDist = v.x * pPlane->m_Normal.x + v.y * pPlane->m_Normal.y +
	        v.z * pPlane->m_Normal.z - pPlane->m_Dist;

	if (fDist > pSphere->m_fRadius)
	{
		*pProject->m_pProjected = 0;

		return WS_INFRONT;
	}

	if (fDist < -pSphere->m_fRadius)
	{
		*pProject->m_pProjected = 0;

		return WS_BEHIND;
	}

	return ws_ProjectBox(pProject);
}

// The six axis-aligned faces. A plane whose normal is an axis direction needs
// no projection at all: the interval is the box's own bounds along that axis,
// and for a negative normal the two ends swap and change sign.
//
// All six are an `else if`/`else` chain rather than three guarded returns,
// and that is the whole difference between a match and a size-exact near.
// Three separate `if (...) return X;` statements let our build cross-jump the
// straddle exit's `pop eax; ret` onto the one the front exit already has; the
// shipped build writes both out. Written as one chain, VC6 emits each exit's
// own copy. Nothing else moved it -- both compiler revisions, /Os without
// /Og, /O2, /Og /Ot and reordering the three exits were all tried and all
// left the cross-jump in place.
int ws_ProjectBoxXPos(CBoxProject* pProject)
{
	pProject->m_fMin = pProject->m_vMin.x - pProject->m_pPlane->m_Dist;
	pProject->m_fMax = pProject->m_vMax.x - pProject->m_pPlane->m_Dist;

	if (pProject->m_fMin <= WS_PLANE_SLOP && pProject->m_fMax > WS_PLANE_SLOP)
		return WS_STRADDLE;
	else if (pProject->m_fMin > WS_PLANE_SLOP)
		return WS_INFRONT;
	else
		return WS_BEHIND;
}

int ws_ProjectBoxXNeg(CBoxProject* pProject)
{
	pProject->m_fMin = -pProject->m_vMax.x - pProject->m_pPlane->m_Dist;
	pProject->m_fMax = -pProject->m_vMin.x - pProject->m_pPlane->m_Dist;

	if (pProject->m_fMin <= WS_PLANE_SLOP && pProject->m_fMax > WS_PLANE_SLOP)
		return WS_STRADDLE;
	else if (pProject->m_fMin > WS_PLANE_SLOP)
		return WS_INFRONT;
	else
		return WS_BEHIND;
}

int ws_ProjectBoxYPos(CBoxProject* pProject)
{
	pProject->m_fMin = pProject->m_vMin.y - pProject->m_pPlane->m_Dist;
	pProject->m_fMax = pProject->m_vMax.y - pProject->m_pPlane->m_Dist;

	if (pProject->m_fMin <= WS_PLANE_SLOP && pProject->m_fMax > WS_PLANE_SLOP)
		return WS_STRADDLE;
	else if (pProject->m_fMin > WS_PLANE_SLOP)
		return WS_INFRONT;
	else
		return WS_BEHIND;
}

int ws_ProjectBoxYNeg(CBoxProject* pProject)
{
	pProject->m_fMin = -pProject->m_vMax.y - pProject->m_pPlane->m_Dist;
	pProject->m_fMax = -pProject->m_vMin.y - pProject->m_pPlane->m_Dist;

	if (pProject->m_fMin <= WS_PLANE_SLOP && pProject->m_fMax > WS_PLANE_SLOP)
		return WS_STRADDLE;
	else if (pProject->m_fMin > WS_PLANE_SLOP)
		return WS_INFRONT;
	else
		return WS_BEHIND;
}

int ws_ProjectBoxZPos(CBoxProject* pProject)
{
	pProject->m_fMin = pProject->m_vMin.z - pProject->m_pPlane->m_Dist;
	pProject->m_fMax = pProject->m_vMax.z - pProject->m_pPlane->m_Dist;

	if (pProject->m_fMin <= WS_PLANE_SLOP && pProject->m_fMax > WS_PLANE_SLOP)
		return WS_STRADDLE;
	else if (pProject->m_fMin > WS_PLANE_SLOP)
		return WS_INFRONT;
	else
		return WS_BEHIND;
}

int ws_ProjectBoxZNeg(CBoxProject* pProject)
{
	pProject->m_fMin = -pProject->m_vMax.z - pProject->m_pPlane->m_Dist;
	pProject->m_fMax = -pProject->m_vMin.z - pProject->m_pPlane->m_Dist;

	if (pProject->m_fMin <= WS_PLANE_SLOP && pProject->m_fMax > WS_PLANE_SLOP)
		return WS_STRADDLE;
	else if (pProject->m_fMin > WS_PLANE_SLOP)
		return WS_INFRONT;
	else
		return WS_BEHIND;
}

float g_BoxSigns[6] = { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };

WSProjectFn g_BoxProjectFns[7] = {
	ws_ProjectBoxXPos, ws_ProjectBoxXNeg,
	ws_ProjectBoxYPos, ws_ProjectBoxYNeg,
	ws_ProjectBoxZPos, ws_ProjectBoxZNeg,
	ws_ProjectBoxCheap
};

// The box has hit a world polygon. Remember it if it is the most upward-
// facing floor seen so far -- that is what the object ends up standing on --
// tell the caller about the impact, then answer the contact either by pushing
// the box clear of the plane or by stopping the move at it, and shift the box
// to wherever that leaves it.
//
// `inline`, defined above its one caller: lithtech.exe's /O2 ws_MoveBox
// (0x00418650, 3189 bytes) carries this body and the client has no copy of
// its own; server.dll's /Os declines it and ws_MoveBox calls 0x10004167.
inline int ws_HitPoly(WorldPoly* pPoly, CBounceState* pState, CPolyTest* pTest)
{
	LTPlane* pPlane = pPoly->GetPlane();

	if (pPlane->m_Normal.y > WS_FLOOR_NORMAL_Y)
	{
		if (!g_pMoveResult->m_pStandingPoly ||
		    g_pMoveResult->m_pStandingPoly->GetPlane()->m_Normal.y <
		        pPlane->m_Normal.y)
		{
			g_pMoveResult->m_pStandingPoly = pPoly;
		}
	}

	ws_HandleImpact(g_pMoveState->m_pInfo, pState, g_pMoveState->m_pObject,
	                g_pMoveState->m_hObject, g_pMoveState->m_pWorldBsp,
	                pPoly, 1);

	pState->m_nHits++;

	if (*pTest->m_Start.m_pProjected == 0)
		ws_ProjectBox(&pTest->m_End);

	if (g_pMoveState->m_bPushOut)
		ws_AddContact(pPlane, pPlane, -pTest->m_End.m_fMin);
	else
		ws_AddClippedContact(pPlane, pPlane, pTest->m_End.m_fMin);

	return ws_ShiftBox();
}

// Is this point inside the world? A plain descent, no stack and no geometry:
// at each node the point goes to whichever side of the plane it falls on, and
// the answer is whichever leaf it lands in. The two leaf bits are tested one
// at a time rather than together, so a node carrying 0x02 answers without
// 0x01 ever being looked at.
int bsp_PointInside(BSPNode* pRoot, LTVector* pPoint)
{
	BSPNode* pNode = pRoot;
	LTVector v;
	float    fDist;
	int      nSide;

	for (;;)
	{
		if (pNode->m_Flags & BSPNODE_LEAF_BIT1)
			return 0;

		if (pNode->m_Flags & BSPNODE_LEAF_BIT0)
			return 1;

		{
			LTPlane* pPlane = pNode->m_pOwner->m_pParent->m_pPlane;

			v = *pPoint;

			fDist = v.y * pPlane->m_Normal.y + v.z * pPlane->m_Normal.z +
			        v.x * pPlane->m_Normal.x - pPlane->m_Dist;
		}

		if (fDist > 0.0f)
			nSide = 1;
		else
			nSide = 0;

		pNode = pNode->m_pChild[nSide];
	}
}

// Is a sphere entirely inside the world, entirely outside it, or across the
// boundary? The walk keeps its own five-hundred-node stack: a node the sphere
// clears on one side is followed down that side alone, and a node it straddles
// is followed down the front with the back side pushed for later — unless the
// back side is a leaf carrying 0x02, which settles the question at once and
// answers two.
//
// Nothing is returned when a leaf is reached, only when the stack runs out:
// the answer is then the last leaf's own 0x01 bit, so a sphere that has been
// entirely on one side of everything reports what that side is.
//
// The if/else below is load-bearing, and it is the whole difference between
// this function and a near. Both arms end in the same "descend the front
// side" tail, which VC6 emits once and jumps to from the other arm. Written
// as a guard —
//
//     if (fDist > fRadius) { pNode = pNode->m_pChild[1]; continue; }
//     ... the straddle work ...
//     pNode = pNode->m_pChild[1];
//
// — the surviving copy is the *last* one and the guard jumps forward to it.
// Written as an explicit if/else, as here, the surviving copy is the *first*
// one and the straddle path jumps backward into it, which is what shipped.
// Same instructions either way, twenty bytes of different placement. This is
// the "shared-exit host" near class and the arm shape moves it.
int bsp_SphereInside(BSPNode* pRoot, LTVector* pPoint, float fRadius)
{
	BSPNode*  pStack[BSP_SPHERE_STACK];
	BSPNode** ppTop = pStack;
	BSPNode*  pNode = pRoot;
	LTVector  v;
	float     fDist;

	for (;;)
	{
		// A `while` rather than `if (...) { ...; continue; }`: the CFG is the
		// same, but VC6 only rotates a loop it was given as one. The shipped
		// /O2 client emits the leaf test twice -- once as the loop's entry
		// test and once at the bottom of the pop body (`test %bl,0x16(%ecx);
		// jne`) -- and puts the 3 in %bl because the constant now has two
		// uses, which is the extra `push %ebx` this function's frame carries.
		// /O1 declines the duplication on size and keeps the single test the
		// `if`/`continue` form gives, so server.dll is unaffected.
		while (pNode->m_Flags & BSPNODE_LEAF)
		{
			if (ppTop == pStack)
				return pNode->m_Flags & BSPNODE_LEAF_BIT0;

			ppTop--;
			pNode = *ppTop;
		}

		{
			LTPlane* pPlane = pNode->m_pOwner->m_pParent->m_pPlane;

			v = *pPoint;

			fDist = v.y * pPlane->m_Normal.y + v.z * pPlane->m_Normal.z +
			        v.x * pPlane->m_Normal.x - pPlane->m_Dist;
		}

		if (fDist > fRadius)
		{
			pNode = pNode->m_pChild[1];
		}
		else
		{
			if (fDist < -fRadius)
			{
				pNode = pNode->m_pChild[0];
				continue;
			}

			{
				BSPNode* pBack = pNode->m_pChild[0];

				if (pBack->m_Flags & BSPNODE_LEAF_BIT1)
					return 2;

				*ppTop = pBack;
				ppTop++;
			}

			pNode = pNode->m_pChild[1];
		}
	}
}

// The walk's entry point, whose whole job is to unpack the query: the two
// ends the caller pointed at are copied onto the stack so the recursion can
// take them by value, and the query travels on beside them. Nothing else
// happens here — the answer is the recursion's, unexamined.
//
// The two twelve-byte copies are the by-value arguments, and the order they
// are written in is the push order: the end vector goes to the higher slot
// first, then the start.
int bspq_IntersectLine(BSPNode* pRoot, CLineQuery* pQuery)
{
	return bspq_IntersectLineRec(pRoot, pQuery, *pQuery->m_pStart,
	                             *pQuery->m_pEnd);
}

// The constructor is `inline`, defined here beside its only user in this
// file (objbank.cpp keeps its own copy of the class and the same body). /O1
// declines to expand it and server.dll calls the COMDAT at 0x1000f08d; /O2
// expands it in lithtech.exe (0x0043c360), where the dead stores to the three
// pointers go and m_pPoly, +0x0c, +0x10 are zeroed in that order.
inline CLineQuery::CLineQuery()
{
	m_pStart    = LTNULL;
	m_pEnd      = LTNULL;
	m_pHit      = LTNULL;
	m_pPoly     = LTNULL;
	m_Unknown0c = 0;
	m_Unknown10 = 0;
}

// The line query, from the outside: build the query object, hand it to the
// walk, and if the walk found something copy the polygon's plane out for the
// caller and answer the polygon. The crossing point itself is written by the
// walk, through the pointer the query carries — this function never touches
// it.
//
// The query is constructed rather than cleared here: the shipped code calls
// the six-store zeroing constructor at 0x1000f08d and only then assigns the
// three pointers, so the two unknown dwords and the result are left at the
// zero the constructor put there.
WorldPoly* bsp_IntersectLine(BSPNode* pRoot, LTVector* pStart, LTVector* pEnd,
                             LTVector* pHit, LTPlane* pPlane)
{
	CLineQuery query;

	query.m_pStart = pStart;
	query.m_pEnd   = pEnd;
	query.m_pHit   = pHit;

	// The success path falls through and the failure return sits at the end
	// with its own `xor eax,eax`: written as `if (!walk) return NULL;` the
	// zero the walk already returned is reused instead, which is two bytes
	// shorter and not what shipped.
	if (bspq_IntersectLine(pRoot, &query))
	{
		*pPlane = *query.m_pPoly->GetPlane();

		return query.m_pPoly;
	}

	return NULL;
}

// Move a point rather than a box. There are no contacts and no box geometry
// to rebuild: the segment is handed to the world, and whatever it hits first
// bounces it and becomes the new end of the segment. Forty of those is as
// much as any point should ever need.
void ws_MovePoint(CMoveState* pMove, CBounceState* pState)
{
	LTVector   vHit;
	LTPlane    planeHit;
	WorldPoly* pPoly;
	int        i;

	pState->m_vPoint = pMove->m_vEnd;

	for (i = 0; i < WS_MAX_POINT_STEPS; i++)
	{
		pPoly = bsp_IntersectLine(pMove->m_pWorldBsp->GetRoot(),
		                          &pMove->m_vStart, &pState->m_vPoint,
		                          &vHit, &planeHit);

		if (!pPoly)
			return;

		ws_HandleImpact(g_pMoveState->m_pInfo, pState, pMove->m_pObject,
		                g_pMoveState->m_hObject, g_pMoveState->m_pWorldBsp,
		                pPoly, 1);

		pState->m_vPoint = vHit;
		pState->m_nHits++;
	}

	if (g_DebugLevel > 0)
	{
		dsi_ServerError("Error: (point) physics recursed infinitely on a %s",
		                pMove->m_pHost->GetObjectName(pMove->m_pObject));
	}
}

// Does this box touch the world? Three point tests and one sphere test settle
// almost every case without building any box geometry at all; only a sphere
// that straddles is worth the full walk. The offset is zeroed first because
// the geometry ws_SetupBoxGeometry lays out is absolute, not relative to a
// move in progress.
int ws_GetBoxStatus(BSPNode* pRoot, LTVector* pMin, LTVector* pMax)
{
	int nStatus;

	g_BoxSphere.m_vPoint = *pMax - *pMin;
	g_BoxSphere.m_vPoint *= 0.5f;

	g_BoxSphere.m_fRadius = g_BoxSphere.m_vPoint.Mag() + 0.1f;

	g_BoxSphere.m_vPoint += *pMin;

	if (bsp_PointInside(pRoot, &g_BoxSphere.m_vPoint) &&
	    bsp_PointInside(pRoot, pMin) &&
	    bsp_PointInside(pRoot, pMax))
	{
		nStatus = bsp_SphereInside(pRoot, &g_BoxSphere.m_vPoint,
		                           g_BoxSphere.m_fRadius);

		if (nStatus == 1)
			return 0;

		if (nStatus)
		{
			// Init, not three stores: the same three `movl $0` either
			// way, but the Init is one more inline expansion, and that is
			// what spends lithtech.exe's /O2 budget exactly where shipped
			// does -- the last two Inits of ws_SetupBoxGeometry below
			// become calls to 0x0041ab50 (0x00417b0c, 0x00417b21), not one.
			g_vMoveBase.Init(0.0f, 0.0f, 0.0f);

			ws_SetupBoxGeometry(pMin, pMax);

			return bspq_TestSphere(pRoot, &g_BoxSphere);
		}
	}

	return 1;
}

// The sweep ran into something. Back its destination out along the surface's
// normal by a little more than it went in, and if the surface is a wall --
// its normal dead level -- reflect the travel along the wall as well, so the
// volume slides rather than stopping dead. Then re-derive the sweep from the
// destination that leaves. A surface too steep to stand on is reported to the
// game; a floor is not, because walking up a slope is not a collision.
void CSimContext::DeflectOffHit(WorldPoly* pPoly, CBounceState* pState)
{
	LTVector vOffset;
	LTVector vSide;

	vOffset = m_vHitNormal * -(m_fHitDepth * WS_PUSHOUT_SCALE);

	if (m_vHitNormal.y == 0.0f)
	{
		vSide = m_vDir.Cross(LTVector(0.0f, 1.0f, 0.0f));

		vOffset += vSide * (vSide.Dot(m_vHitNormal) * -2.0f);
	}

	m_vEnd += vOffset;

	if (m_bReport && m_vHitNormal.y < WS_SLOPE_LIMIT)
	{
		g_pMoveResult->m_pStandingPoly = m_pHitPoly;

		ws_HandleImpact(g_pMoveState->m_pInfo, pState,
		                g_pMoveState->m_pObject, g_pMoveState->m_hObject,
		                g_pMoveState->m_pWorldBsp, pPoly, 1);
	}

	Setup();
}

// Two objects have run into each other. Each one's share of the collision is
// the part of its velocity that was heading into the other, turned around;
// the strength the game is told about is the difference between the two
// shares, weighted by the pair's mass. An object only hears about it if it
// asked to and the hit was hard enough to be worth hearing about, and only
// the object with the lower blocking priority actually gets moved -- which is
// what makes a lift push a player rather than the other way round.
//
// Byte-identical on lithtech.exe (0x41a890, 689 bytes). The two impulses and
// the two dot products are each a two-element ARRAY, not a pair of scalars.
// That is what the frames were saying: shipped server (0x100052d0) puts the
// compiler temporaries at the top of the frame and the impulses lowest
// (vImpulse[0] at -0x4c, [1] at -0x40), and its four dwords run pInfo,
// pVel2, fDot[1], fDot[0] from -0x4 down -- the arrays, being the larger
// objects, sit below the scalars. With separate locals ours was the mirror
// image, and lithtech (frame 0x38 against our 0x34) homed fDot1/fDot2 in the
// dead pObj1/pObj2 parameter slots where shipped keeps them in the frame and
// puts pVel2 in pObj1's slot instead; an array element cannot be homed in a
// parameter slot. Both arrays are needed -- either alone leaves lithtech off.
// Naming the first object's velocity in a pointer local *inside* the if that
// adds to it is also part of the shape (shipped forms that address with a
// `lea` there and only there; the dot product at the top reads the member).
// NEAR on server.dll (562 of 562, one instruction scheduled differently): the
// loads of pInfo and fDist for the three pInfo stores come after ONE movsl of
// the vImpulse[1] copy in shipped, after two in ours. Store order of those
// three statements, pVel2's position, declaration order and Dot(*pVel2) do
// not move it.
void ws_ObjectsCollided(IPhysicsSim* pSim, LTObject* pObj1, LTObject* pObj2,
                        LTVector* pNormal, float fDist)
{
	LTVector       vImpulse[2];
	LTVector       vRel;
	LTVector       vNormal2;
	CollisionInfo* pInfo;
	LTVector*      pVel2;
	float          fDot[2];

	pInfo = pSim->GetOwner()->m_pInfo;

	fDot[0] = pObj1->m_Velocity.x * pNormal->x +
	          pObj1->m_Velocity.y * pNormal->y +
	          pObj1->m_Velocity.z * pNormal->z;

	vImpulse[0] = *pNormal * -fDot[0];

	vNormal2 = -(*pNormal);

	pVel2 = &pObj2->m_Velocity;

	fDot[1] = vNormal2.Dot(pObj2->m_Velocity);

	vImpulse[1] = vNormal2 * -fDot[1];

	pInfo->m_Plane.m_Normal = *pNormal;
	pInfo->m_hPoly = 0;
	pInfo->m_Plane.m_Dist = fDist;

	vRel = (vImpulse[0] - vImpulse[1]) *
	       ((pObj1->m_Mass + pObj2->m_Mass) * WS_IMPULSE);

	{
		float fMagSqr = vRel.MagSqr();
		float fMag    = (float)sqrt(fMagSqr);

		if ((pObj1->m_Flags & FLAG_TOUCH_NOTIFY) &&
		    fMagSqr >= pObj1->m_ForceIgnoreLimitSqr)
		{
			pSim->OnBounce(pObj1, pObj2, &vImpulse[0], fMag);
		}

		if ((pObj2->m_Flags & FLAG_TOUCH_NOTIFY) &&
		    fMagSqr >= pObj2->m_ForceIgnoreLimitSqr)
		{
			pSim->OnBounce(pObj2, pObj1, &vImpulse[1], -fMag);
		}
	}

	if (fDot[0] < 0.0f &&
	    pObj1->m_BlockingPriority <= pObj2->m_BlockingPriority)
	{
		LTVector* pVel1 = &pObj1->m_Velocity;

		pObj1->m_ChangeFlags |= CHANGEFLAG_MOTION;

		pVel1->x += vImpulse[0].x;
		pVel1->y += vImpulse[0].y;
		pVel1->z += vImpulse[0].z;
	}

	if (fDot[1] < 0.0f &&
	    pObj2->m_BlockingPriority <= pObj1->m_BlockingPriority)
	{
		pObj2->m_ChangeFlags |= CHANGEFLAG_MOTION;

		pVel2->x += vImpulse[1].x;
		pVel2->y += vImpulse[1].y;
		pVel2->z += vImpulse[1].z;
	}
}

// Everything the move could possibly touch, as one box: the span between
// where the move wanted to go and where it has been pushed to, widened by the
// object's own half-extents at both ends. The bounding sphere gets a metre of
// slack on top, because it is only ever used to reject, never to accept.
// NEAR (server 484 of 485, lithtech 624 of 626): the shipped server spills
// pMax->z to the dead pMax slot (`fsts 0xc(%ebp)`, 0x100037ed) before the
// `+=` and adds max-first; lithtech 0x4182b6 adds max-first too. Changing
// vCentre to `*pMin + vHalf` (or Init, or straight into g_BoxSphere) fixes
// the server's second half and leaves only that spill, but moves lithtech's
// SetupBoxGeometry pushes above the half multiplies (630 bytes), so it is
// not taken. Several spellings of the `+=` make lithtech inline
// SetupBoxGeometry, which shipped does not.
void ws_SetupSweptBox(LTVector* pMin, LTVector* pMax)
{
	LTVector vCentre;
	LTVector vHalf;

	VEC_MIN(*pMin, g_vMoveStart, g_vMoveEnd);
	VEC_MAX(*pMax, g_vMoveStart, g_vMoveEnd);

	*pMax += g_vMoveBase + g_pMoveState->m_vDims;
	*pMin += g_vMoveBase - g_pMoveState->m_vDims;

	vHalf = (*pMax - *pMin) * 0.5f;

	vCentre.x = pMin->x + vHalf.x;
	vCentre.y = pMin->y + vHalf.y;
	vCentre.z = pMin->z + vHalf.z;

	g_BoxSphere.m_vPoint = vCentre;
	g_BoxSphere.m_fRadius = vHalf.Mag() + 1.0f;

	ws_SetupBoxGeometry(pMin, pMax);
}

// The solver has pushed the box somewhere new, so everything derived from
// where it was has to be rebuilt: the sixteen points that bound the sweep,
// the three bounding spheres, and the six clip-plane distances. Answers zero
// and touches nothing when the push came to nothing, which is what stops the
// caller going round again.
//
// NEAR, size-exact on both images: server 0x100022ac (907 bytes, 308
// instructions) and lithtech 0x4165c0 (1049 bytes, 302). Three shapes got it
// there:
//   * `vHalf = vLen * 0.5f; vMid = vP1 + vHalf;` as operators. Written as
//     component stores, VC6 kept vP1.x and vP1.z on the FPU stack across the
//     loop (`fsts`, then `fadd %st(3)`), which also swapped vP1 and vP2's
//     frame slots. Shipped keeps vHalf.x on the stack and never stores it.
//   * the early-out tests vLen, not vDiff: shipped reads x through the copy
//     it has just made (-0x30) and y and z from vLen (-0x44, -0x40).
//   * the loop writes through `pOut = g_SweepPoints[i]`. That anchors the
//     induction pointer at [i][1].z and moves `add $0x60` into the middle of
//     the body, as shipped has it. Reading vBase[i] directly rather than
//     through a pointer local takes lithtech from 20 differing instructions
//     to 12.
// What is left: the operand order of the commutative `fadds` in the corner
// sums (shipped loads dims first for x and y on server, and for y and z on
// lithtech), plus one instruction scheduled differently on each image
// (server's `mov $g_SweepPoints+0x14,%edx`, lithtech's `sub $0x3c,%esp`).
// Swapping terms in the source does nothing. `LTVector* pD =
// &g_pMoveState->m_vDims` declared before the vBase pointer fixes the server
// order (4 instructions left) but costs lithtech 5 bytes: shipped lithtech
// addresses dims.y/z off g_pMoveState and dims.x through `lea 0x2c`, which is
// the CSE of the member form used here.
int ws_ShiftBox()
{
#ifdef LT_SERVER_TIMERS
	CountAdder  cntAdder(&g_nPhysMoveTime);
#endif
	CMoveState* pState;
	LTVector    vBase[2];
	LTVector    vMax;
	LTVector    vMin;
	LTVector    vP2;
	LTVector    vP1;
	LTVector    vDiff;
	LTVector    vLen;
	LTVector    vMid;
	LTVector    vHalf;
	uint32      i;

	vDiff.x = g_vMoveEnd.x - g_vMoveStart.x;
	vDiff.y = g_vMoveEnd.y - g_vMoveStart.y;
	vDiff.z = g_vMoveEnd.z - g_vMoveStart.z;

	vLen = vDiff;

	if (fabs(vLen.x) < WS_MOVE_EPSILON &&
	    fabs(vLen.y) < WS_MOVE_EPSILON &&
	    fabs(vLen.z) < WS_MOVE_EPSILON)
	{
		return 0;
	}

	pState = g_pMoveState;

	vP1.x = g_vMoveBase.x + g_vMoveStart.x;
	vP1.y = g_vMoveBase.y + g_vMoveStart.y;
	vP1.z = g_vMoveBase.z + g_vMoveStart.z;

	vBase[0] = vP1;

	vP2.x = g_vMoveBase.x + g_vMoveEnd.x;
	vP2.y = g_vMoveBase.y + g_vMoveEnd.y;
	vP2.z = g_vMoveBase.z + g_vMoveEnd.z;

	vBase[1] = vP2;

	for (i = 0; i < 2; i++)
	{
		LTVector* pOut = g_SweepPoints[i];

		pOut[0].Init(vBase[i].x - g_pMoveState->m_vDims.x,
		             vBase[i].y + g_pMoveState->m_vDims.y,
		             vBase[i].z - g_pMoveState->m_vDims.z);
		pOut[1].Init(vBase[i].x - g_pMoveState->m_vDims.x,
		             vBase[i].y + g_pMoveState->m_vDims.y,
		             vBase[i].z + g_pMoveState->m_vDims.z);
		pOut[2].Init(vBase[i].x + g_pMoveState->m_vDims.x,
		             vBase[i].y + g_pMoveState->m_vDims.y,
		             vBase[i].z + g_pMoveState->m_vDims.z);
		pOut[3].Init(vBase[i].x + g_pMoveState->m_vDims.x,
		             vBase[i].y + g_pMoveState->m_vDims.y,
		             vBase[i].z - g_pMoveState->m_vDims.z);
		pOut[4].Init(vBase[i].x - g_pMoveState->m_vDims.x,
		             vBase[i].y - g_pMoveState->m_vDims.y,
		             vBase[i].z - g_pMoveState->m_vDims.z);
		pOut[5].Init(vBase[i].x - g_pMoveState->m_vDims.x,
		             vBase[i].y - g_pMoveState->m_vDims.y,
		             vBase[i].z + g_pMoveState->m_vDims.z);
		pOut[6].Init(vBase[i].x + g_pMoveState->m_vDims.x,
		             vBase[i].y - g_pMoveState->m_vDims.y,
		             vBase[i].z + g_pMoveState->m_vDims.z);
		pOut[7].Init(vBase[i].x + g_pMoveState->m_vDims.x,
		             vBase[i].y - g_pMoveState->m_vDims.y,
		             vBase[i].z - g_pMoveState->m_vDims.z);
	}

	g_SphereAtDelta.m_vPoint = vP1;
	g_SphereAtOffset.m_vPoint = vP2;
	g_SphereAtOffset.m_fRadius = g_fBoxRadius;
	g_SphereAtDelta.m_fRadius = g_fBoxRadius;

	vHalf = vLen * 0.5f;

	vMid = vP1 + vHalf;

	g_BoxSphere.m_vPoint = vMid;
	g_BoxSphere.m_fRadius = vLen.Mag() + g_fBoxRadius;

	VEC_MIN(vMin, vP1, vP2);
	VEC_MAX(vMax, vP1, vP2);

	vMin -= g_pMoveState->m_vDims;
	vMax += g_pMoveState->m_vDims;

	g_BoxPlanes[0].m_Dist =  vMin.x;
	g_BoxPlanes[1].m_Dist = -vMax.x;
	g_BoxPlanes[2].m_Dist =  vMin.y;
	g_BoxPlanes[3].m_Dist = -vMax.y;
	g_BoxPlanes[4].m_Dist =  vMin.z;
	g_BoxPlanes[5].m_Dist = -vMax.z;

	return 1;
}

// The move, from the outside. Everything else in the module runs under this.
// The state it is given is copied, because the module works on one global move
// and a move can start another one before it finishes -- the old pointers are
// put back on the way out, which is what makes the recursion safe.
//
// An object flagged for point collision skips all of it: no box, no contacts,
// just a line through the world. Everything else gets up to forty attempts,
// each one earned by the last having hit something new -- except a cylinder
// mover, which gets one: both images re-read the hit count right after
// ws_MoveCylinder (server 0x10003690, lithtech 0x4180ad), so the loop's test
// always ends it. The timing counter exists only in the server build
// (lithtech 0x417da0 has no CBaseCounter), and fStepHeight lives only in the
// step-up block, which is what lets it share bStepped's slot as shipped.
// Each global is saved and replaced in turn (state, then result): lithtech
// 0x417deb reads g_pMoveResult only after the copy, and with both saves
// written first /O2 hoists that read above the rep movs.
void ws_MoveObject(CMoveState* pIn, CBounceState* pState)
{
#ifdef LT_SERVER_TIMERS
	CBaseCounter  counter(0);
#endif
	CMoveState    state;
	LTVector      vMax;
	LTVector      vMin;
	uint32        nOldHits;
	CBounceState* pOldResult;
	CMoveState*   pOldState;
	int           nTries;
	int           bStepped;
	uint32        nHits;

	state.m_n44 = 0;

	g_nPhysStatA8 = 0;
	g_nPhysMoveTime = 0;
	g_nPhysStatA4 = 0;
	g_nPhysContactTime = 0;
	g_nPhysStepTime = 0;
	g_nPhysMoves = 0;
	g_nPhysNodes = 0;

#ifdef LT_SERVER_TIMERS
	cnt_StartCounter(&counter);
#endif

	state = *pIn;

	pOldState     = g_pMoveState;
	g_pMoveState  = &state;
	pOldResult    = g_pMoveResult;
	g_pMoveResult = pState;

	pState->m_pStandingPoly = NULL;
	pState->m_vMomentum.Init();
	pState->m_vVelocityAdd.Init();
	pState->m_nHits = 0;

	if (pIn->m_pObject->m_Flags & FLAG_POINTCOLLIDE)
	{
		ws_MovePoint(pIn, pState);

		g_pMoveState  = pOldState;
		g_pMoveResult = pOldResult;

		return;
	}

	g_nContacts = 0;

	g_vMoveStart = pIn->m_vStart;
	g_vMoveEnd   = pIn->m_vEnd;

	g_vMoveBase.x = 0.0f;
	g_vMoveBase.y = 0.0f;
	g_vMoveBase.z = 0.0f;

	g_pPBlock = g_pMoveState->m_pWorldBsp->GetPBlock(g_vMoveStart);

	if (!g_pPBlock)
	{
		dsi_ServerError("World %s doesn't have PBlocks!",
		                g_pMoveState->m_pWorldBsp->m_Name);

		g_pMoveState  = pOldState;
		g_pMoveResult = pOldResult;

		return;
	}

	bStepped = 0;

	if (pIn->m_n38)
	{
		float fStepHeight;

		pIn->m_pHost->GetStepper()->GetStepHeight(&fStepHeight);

		if (fStepHeight < 0.0f)
			fStepHeight = pIn->m_vDims.y * 0.5f;

		fStepHeight = (fStepHeight + 0.5f) * 0.5f;

		g_vMoveBase.y = -(pIn->m_vDims.y - fStepHeight);

		g_pMoveState->m_vDims = pIn->m_vDims;
		g_pMoveState->m_vDims.y = fStepHeight;

		nOldHits = pState->m_nHits;

		ws_SetupSweptBox(&vMin, &vMax);

		bStepped = ws_StairStep(g_pPBlock->m_nIndex, pState);

		g_pMoveState->m_vDims = pIn->m_vDims;

		if (bStepped)
		{
			g_vMoveBase.y = 0.0f;
		}
		else
		{
			g_vMoveBase.y = fStepHeight;

			g_pMoveState->m_vDims.y -= fStepHeight;
		}

		if (pState->m_nHits != nOldHits && !g_pMoveState->m_n44)
			bStepped = 1;
		else
			bStepped = 0;

		pState->m_nHits = nOldHits;
	}

	g_fBoxRadius = g_pMoveState->m_vDims.Mag() + 0.01f;

	for (nTries = 0; nTries < WS_MAX_MOVE_STEPS; nTries++)
	{
		nHits = pState->m_nHits;

		if (ws_ShiftBox())
		{
			if (g_pMoveState->m_pObject->m_Flags2 & FLAG2_PLAYERCOLLIDE)
			{
				ws_MoveCylinder(g_pPBlock->m_nIndex, pState, nTries > 0);
				nHits = pState->m_nHits;
			}
			else
				ws_MoveBox(g_pPBlock->m_nIndex, pState, nTries > 0);
		}

		if (nHits == pState->m_nHits)
			break;
	}

	if (nTries == WS_MAX_MOVE_STEPS)
	{
		if (g_DebugLevel > 0)
		{
			dsi_ServerError("Error: physics recursed infinitely on a %s",
			                g_pMoveState->m_pHost->GetObjectName(
			                    pIn->m_pObject));
		}

		g_vMoveEnd = g_vMoveStart;
	}

	pState->m_vPoint = g_vMoveEnd;

	g_pMoveState  = pOldState;
	g_pMoveResult = pOldResult;

	if (pState->m_nHits == 0 && bStepped)
		pState->m_nHits = 1;
}

// Does the swept cylinder reach this polygon? The cheap rejections come
// first: a plane the sweep is moving away from cannot be hit, and neither can
// a polygon whose bounding sphere is out of reach. What follows is the
// expensive part -- walk the polygon's edges, find the point on its boundary
// closest to where the cylinder ends up, and decide whether that point is
// inside the polygon or on one of its edges. A face hit is answered with the
// plane's own normal; an edge hit with the direction from the edge back to
// the cylinder's axis, which is what makes a cylinder slide round a corner
// instead of catching on it.
//
// NEAR (1378 of 1388 bytes, 509 instructions against 512, 3 mnemonic
// blocks; see the final changes at the end of this comment). Things settled and
// worth not re-deriving:
//
//   * the first comparison really is against a *double*. 0x1004d228 reads
//     0x3f847ae147ae147b, which is 0.01 exactly, and the shipped instruction
//     is `fcompl`; so `m_vDir.Dot(...) > 0.01` with no `f` suffix is right,
//     and "fixing" it to 0.01f gives `fcomps` and is wrong.
//   * both `fabs` comparisons are the other way round: the shipped code
//     calls fabs and then compares with `fcomps` against a float constant,
//     which C's own promotion rules cannot give (fabs returns double, so the
//     float would widen). They need an explicit `(float)` on the fabs.
//
// Four more source facts read off the shipped bytes:
//
//   * the vertical clamp of the polygon centre is NOT `LTCLAMP`. The shipped
//     build reloads the centre for each comparison (`flds; fcomps m_fMin`,
//     then again against m_fMax) and moves the selected float into the
//     destination as *integer* bits through %eax with one shared store —
//     which is three separate assignments in an if/else-if/else, not one
//     assignment whose value is a conditional expression. The macro form put
//     both operands on the FPU stack (`flds; fcompp`) and inverted every
//     branch with them. Note this is the opposite way round from
//     CSimContext::Setup's extents, which did want the macro.
//   * the bounding-sphere reject names its radius sum. `m_fReach +
//     pPoly->m_fRadius` written twice inside the comparison leaves the load
//     after the m_vMid copy; a named local puts the `flds` before it, where
//     the shipped code has it. (The order of the three squared differences
//     is not a source fact: writing the sum x,y,z and y,z,x emits the
//     identical instructions.)
//   * one `Mag()` call, not two. The shipped `fcoms` against fClosest does
//     not pop, because the taken arm stores the same value it just compared;
//     `if (vDelta.Mag() < fClosest) fClosest = vDelta.Mag();` is two calls
//     and a `fcomps`.
//   * the two edge slopes are locals, and the drop is scoped to its clamp.
//     `vEdge.x / vEdge.y` and `vEdge.z / vEdge.y` are computed once above
//     both clamps and stay on the FPU stack across them, which is what the
//     two `fmul %st(3),%st` at the same stack index mean; and the drop is a
//     block-local of each clamp, so it lives at st(0) (`fld %st(0)` to
//     duplicate it) instead of being spilled to the function-scope fDrop.
//
// The two early rejects are nested positive guards around the
// whole body, with one `return 0` after it. lithtech.exe's copy (0x00416de0,
// really 0x00416de0..0x004176b5, 2261 bytes; the enumerated 143 is an extent
// error) takes both `je` straight to one shared `xor %eax,%eax` epilogue at
// 0x004176a6, and ours had copied the epilogue into each reject. Measured:
// lithtech 2174 bytes / 608 instructions -> 2270 / 629 (shipped 2261 / 631);
// server 23 mnemonic blocks -> 20 (1377 / 504 against 1388 / 512 before and
// after). What remains on lithtech is register allocation: shipped pushes
// %ebp on entry and keeps pPoly in it, where ours shrink-wraps %ebp and uses
// %ebx. There is also FP scheduling through the vertex loop.
//
// The final changes (server 1392/506, 18 blocks -> 1378/509, 3 blocks; lithtech
// 62 blocks -> 24). Each item is read off the shipped bytes:
//   * the edge test is an LTPlane. `fsts -0x80(%ebp)` at 0x10002d2f
//     stores fEdgeDist although nothing reads it back, which a float local
//     would not do. -0x8c..-0x80 is one 16-byte object, so the frame is 0xb0.
//   * the sphere reject is `vD = pPoly->m_vCentre - m_vMid`. The by-value
//     copy of m_vMid is the movsl at 0x1000299b, and the differences load
//     x,y,z.
//   * the vertex loop indexes `m_Vertices[i]` and has no pointer local. The
//     pointer at -0x34 is VC6's strength-reduced induction variable. It is
//     set up after the guard (0x10002af7) and stepped after `i++`
//     (0x10002d75).
//   * three fld/fstp copies are Init(): vFlat (0x10002b67), vOn = vCur
//     (0x10002bf4), and both m_vHitNormal arms (0x10002e0f, whose right-to-left
//     arguments compute .z first, and 0x10002e38).
// Left: `fsubs 0x1c(%esi)` (fDrop) is scheduled one compare earlier in the
// shipped build. The shipped build also keeps two copies of the slope pops
// after the second clamp (0x10002c8f/0x10002c95) where ours merges them.
// Plus frame-slot rotation: vPrev/vFlat, i/bStepUp/the induction pointer,
// and the cross temp against vFlatDelta. bFlatPlane-first did not move the
// fsubs, and neither did function-scope slopes or an empty else.
int CSimContext::TestPoly(WorldPolyOwner* pPoly, WorldPoly* pHandle)
{
	float     fClosest;
	LTVector  vOn;
	LTVector  vClosest;
	LTPlane*  pPlane;
	int       bInsidePoly;
	LTVector  vCur;
	LTVector  vEdge;
	int       bStepUp;
	uint32    i;
	LTVector  vPrev;
	LTVector  vOnPlane;
	int       bFlatPlane;
	LTVector  vDelta;
	LTPlane   edgePlane;
	float     fFlatDist;
	int       nPrev, nCur;
	float     fT, fMag, fInv, fDrop;
	LTVector  vFlatDelta;

	pPlane = pPoly->m_pParent->m_pPlane;

	if (!(m_vDir.Dot(pPlane->m_Normal) > 0.01))
	{
		float    fReach = m_fReach + pPoly->m_fRadius;
		LTVector vD     = pPoly->m_vCentre - m_vMid;

		if (!(vD.x * vD.x + vD.y * vD.y + vD.z * vD.z > fReach * fReach))
		{

	{
		LTVector vPoint;
		LTVector vScaled = pPlane->m_Normal * -m_fPlaneDist;

		vPoint.x = vScaled.x + m_vEnd.x;
		vPoint.y = vScaled.y + m_vEnd.y;
		vPoint.z = vScaled.z + m_vEnd.z;

		vOnPlane = vPoint;
	}

	if ((float)fabs(pPlane->m_Normal.y) < 0.01f)
	{
		if (pPoly->m_vCentre.y < m_fMin)
			vOnPlane.y = m_fMin;
		else if (pPoly->m_vCentre.y > m_fMax)
			vOnPlane.y = m_fMax;
		else
			vOnPlane.y = pPoly->m_vCentre.y;
	}

	bInsidePoly = 1;
	bFlatPlane = 0;
	bStepUp = 0;

	if ((float)fabs(pPlane->m_Normal.y) >= 0.9f)
	{
		fDrop = m_vEnd.y - pPoly->m_vCentre.y;

		bFlatPlane = 1;

		if (m_bReport && pPlane->m_Normal.y > 0.0f && fDrop >= 0.0f &&
		    m_fHeight - fDrop < m_fHeight * 0.5f)
		{
			bStepUp = 1;
		}
		else
		{
			bStepUp = 0;
		}
	}

	vPrev    = *pPoly->m_Vertices[pPoly->m_nVertices - 1].m_pPos;
	vClosest = *pPoly->m_Vertices[pPoly->m_nVertices - 1].m_pPos;

	fClosest = m_fCornerDist;

	for (i = 0; i < pPoly->m_nVertices; i++)
	{
		vCur = *pPoly->m_Vertices[i].m_pPos;

		{
			LTVector vTmp;

			vTmp.x = vCur.x - vPrev.x;
			vTmp.y = vCur.y - vPrev.y;
			vTmp.z = vCur.z - vPrev.z;

			vEdge = vTmp;
		}

		nPrev = ClassifyAxis(vPrev.y);
		nCur  = ClassifyAxis(vCur.y);

		if (nPrev != nCur || nPrev == WS_INSIDE)
		{
			LTVector vFlat;

			vFlat.Init(vEdge.x, 0.0f, vEdge.z);

			fMag = vFlat.Mag();

			if (fMag > 0.01f)
			{
				fInv = 1.0f / fMag;

				fT = ((m_vEnd.z - vPrev.z) * (vFlat.z * fInv) +
				      (m_vEnd.x - vPrev.x) * (vFlat.x * fInv)) * fInv;
			}
			else
			{
				fT = 1.01f;
			}

			if (fT >= 0.0f && fT <= 1.0f)
			{
				vOn.x = fT * vEdge.x + vPrev.x;
				vOn.y = fT * vEdge.y + vPrev.y;
				vOn.z = fT * vFlat.z + vPrev.z;
			}
			else
			{
				vOn.Init(vCur.x, vCur.y, vCur.z);
			}

			if (vEdge.y != 0.0f && (nPrev || nCur))
			{
				float fSlopeX, fSlopeZ;

				vEdge.Norm();

				fSlopeX = vEdge.x / vEdge.y;
				fSlopeZ = vEdge.z / vEdge.y;

				if (vOn.y < m_fMin)
				{
					float fLift = m_fMin - vOn.y;

					vOn.y = m_fMin;

					vOn.x += fLift * fSlopeX;
					vOn.z += fLift * fSlopeZ;
				}

				if (vOn.y > m_fHeight + m_vEnd.y)
				{
					float fLift = m_fMax - vOn.y;

					vOn.y = m_fMax;

					vOn.x += fLift * fSlopeX;
					vOn.z += fLift * fSlopeZ;
				}
			}

			vDelta.x = vOn.x - m_vEnd.x;
			vDelta.y = 0.0f;
			vDelta.z = vOn.z - m_vEnd.z;

			{
				float fDist = vDelta.Mag();

				if (fDist < fClosest)
				{
					fClosest = fDist;
					vClosest = vOn;
				}
			}
		}

		if (bInsidePoly)
		{
			edgePlane.m_Normal = vEdge.Cross(pPlane->m_Normal);
			edgePlane.m_Normal.Norm();

			edgePlane.m_Dist = edgePlane.m_Normal.Dot(vCur);

			// `< -0.01f`, NOT `>= -0.01f`. 0x10002d5e is `jae 0x10002d64`
			// jumping OVER the `and [ebp-0x24],0x0` that clears the flag,
			// so the clear happens on the BELOW branch. This is a semantic
			// fault in the reconstruction, not a codegen shape -- the point
			// was being rejected as outside the polygon in exactly the cases
			// the shipped engine accepts it.
			if (edgePlane.m_Normal.Dot(vOnPlane) - edgePlane.m_Dist < -0.01f)
				bInsidePoly = 0;
		}

		vPrev = vCur;
	}

	vDelta.x = vOnPlane.x - m_vEnd.x;
	vDelta.y = 0.0f;
	vDelta.z = vOnPlane.z - m_vEnd.z;

	vFlatDelta = vDelta;

	fFlatDist = vFlatDelta.Mag();

	if (fClosest < m_fRadius &&
	    !(bInsidePoly && !bFlatPlane && fClosest > fFlatDist) && !bStepUp)
	{
		if (m_fRadius - fClosest <= m_fHitDepth)
			return 1;

		m_vHitPoint = vClosest;

		if (fClosest != 0.0f)
		{
			m_vHitNormal.Init(vClosest.x - m_vEnd.x, 0.0f,
			                  vClosest.z - m_vEnd.z);

			m_vHitNormal.Norm();
		}
		else
		{
			m_vHitNormal.Init(0.0f, 0.0f, 0.0f);
		}

		m_fHitDepth = m_fRadius - fClosest;
	}
	else
	{
		if (!bInsidePoly)
			return 0;

		if (m_fOverlap <= m_fHitDepth)
			return 1;

		m_vHitPoint = vOnPlane;

		m_vHitNormal = -(pPlane->m_Normal);

		m_fHitDepth = m_fOverlap;
	}

	m_pHitPoly = pHandle;

	return 1;
		}
	}

	return 0;
}

// March the cylinder along its path a step at a time, testing the world's
// tree at each step. The step length is the cylinder's own size along the
// direction of travel, so it can never pass through anything thinner than
// itself; when the move is shorter than one step it is done in one. Hitting
// something spends one of three deflections: the destination is backed out of
// whatever was hit and the same step is tried again. Run out of deflections
// with the object still stuck and the whole move is abandoned back to where
// it started.
//
// NEAR on both images: server 1472 of 1472 bytes with 2 mnemonic blocks
// left, lithtech 1987 of 1983 bytes with the same 502 instructions and 17
// blocks left. Each fix below is something the shipped bytes show:
//  * LTMIN(x, z), not (z, x): x is loaded first (0x10004768, 0x00419972).
//  * the CountAdder is built after the three counter stores (0x10004735).
//  * the tree walk reuses nBlock as its node, as ws_StairStep does. The
//    copy into nRoot (0x1000471c, 0x0041993e) then leaves the parameter
//    dead, so VC6 puts fRemaining in its slot (`fsts 0x8(%ebp)`).
//  * vFlat.Init(...): its components go through the FPU (0x10004851).
//  * fDiag = height*2 is set before the test and kept in st across it. The
//    radius arm is the fall-through (0x100048d0, 0x00419b3d).
//  * the second ClassifyPlane's plane is a statement of its own, loaded
//    into a register before the flag (0x00419ddb).
//  * the STRADDLE arm is the `if` and the child arm the `else`
//    (0x00419e10 jne to 0x00419e9f).
//  * the dot products are written z,y,x. VC6 evaluates the first two terms
//    swapped, and the bytes read y first (0x00419d6f, 0x10004a92).
//  * each step size has its own scaled vector, and VC6 packs them into dead
//    slots (-0x5c, -0x40, -0x88 on the server image).
//  * the destination goes into sim.m_vDest, CSimContext +0x18
//    (0x10004954), not into a local.
//  * `vAt = vTmp` comes before the fRemaining update. Otherwise its three
//    movsl cross-jump with the pre-loop copy.
// Left: on the server, VC6 keeps g_vMoveBase.x in st from vFrom.x to vTo.x
// where shipped reloads it. Six spellings of that code (term order, Init,
// VEC_ADD, operator+, statement order) give the same output. On lithtech,
// the frame slot sharing is different (shipped: fRemaining 0x8, fAbsY/
// fStepShort 0xc, fFlat/fStepLong 0x10, fSide 0x50) and fDiag gets a slot
// where shipped keeps it in st. That is FP scheduling and slot assignment
// through the whole frame.
int ws_MoveCylinder(uint16 nBlock, CBounceState* pState, int bRetry)
{
	uint16        nStack[WS_NODE_STACK];
	CSimContext   sim;
	LTVector      vNext;
	LTVector      vStep;
	LTVector      vScaled;
	LTVector      vScaledA;
	LTVector      vScaledB;
	LTVector      vFlat;
	uint16        nRoot;
	WorldPolyRef* pRecs;
	LTVector      vDelta;
	uint32        nHitsAtStart;
	LTVector      vTo;
	LTVector      vFrom;
	uint32        nHitsLast;
	LTVector      vTmp;
	LTVector      vAt;
	int           nDeflects;
	LTVector      vDir;
	float         fStepShort;
	float         fStepLong;
	float         fSide;

	uint16*       pTop;
	WSNode*       pNode;
	WorldPolyRef* pRec;
	LTPlane*      pPlane;
	int           nStart, nEnd, nSide;
	float         fDist, fRemaining, fFlat, fAbsY, fDiag;

	nRoot = nBlock;

	nHitsAtStart = pState->m_nHits;
	nHitsLast    = pState->m_nHits;

	nDeflects = 3;

#ifdef LT_SERVER_TIMERS
	CountAdder    cntAdder(&g_nPhysStepTime);
#endif

	pRecs = g_pMoveState->m_pWorldBsp->GetPolyRefs();

	sim.m_bReport = 0;

	sim.m_fRadius = LTMIN(g_pMoveState->m_vDims.x, g_pMoveState->m_vDims.z);
	sim.m_fHeight = g_pMoveState->m_vDims.y;

	// FIX (a): four nested positive guards, one shared `return 1` below.
	// 0x004199ac / 0x004199c4 / 0x00419aab / 0x00419bc1 all jne 0x0041a0d1.
	if (sim.m_fRadius >= 0.01f)
	{
	if (sim.m_fHeight >= 0.01f)
	{
		vFrom.x = g_vMoveBase.x + g_vMoveStart.x;
		vFrom.y = g_vMoveBase.y + g_vMoveStart.y;
		vFrom.z = g_vMoveBase.z + g_vMoveStart.z;

		vAt = vFrom;

		vTo.x = g_vMoveBase.x + g_vMoveEnd.x;
		vTo.y = g_vMoveBase.y + g_vMoveEnd.y;
		vTo.z = g_vMoveBase.z + g_vMoveEnd.z;

		vDelta.x = vTo.x - vFrom.x;
		vDelta.y = vTo.y - vFrom.y;
		vDelta.z = vTo.z - vFrom.z;

		vDir = vDelta;

		fRemaining = vDir.Mag();

	if (fRemaining != 0.0f)
	{
		vFlat.Init(vDir.x, 0.0f, vDir.z);

		fFlat = vFlat.Mag();

		fAbsY = (float)fabs(vDir.y);

		fSide = 1.0f / fRemaining;

		vDir.x = fSide * vDir.x;
		vDir.y = fSide * vDir.y;
		vDir.z = fSide * vDir.z;

		if (fAbsY <= sim.m_fHeight * 1.8f && sim.m_fRadius * 0.9f >= fFlat)
		{
			fStepShort = fRemaining;
			fStepLong  = fRemaining;
		}
		else
		{
			fDiag = sim.m_fHeight * 2.0f;

			if (fFlat / sim.m_fRadius > fAbsY / fDiag)
			{
				fSide = sim.m_fRadius;
				fDiag = (sim.m_fRadius / fFlat) * fAbsY;
			}
			else
			{
				fSide = (fDiag / fAbsY) * fFlat;
			}

			fDiag = (float)sqrt(fDiag * fDiag + fSide * fSide);

			fStepShort = fDiag * 0.75f;
			fStepLong  = fDiag * 0.9f;
		}

	if (fStepShort > 0.001f)
	{
		sim.m_vDest = vTo;

		for (;;)
		{
			if (fRemaining <= 0.0f)
				break;

			if (nHitsAtStart == pState->m_nHits)
			{
				if (fRemaining < fStepShort)
				{
					vScaledA.x = vDir.x * fRemaining;
					vScaledA.y = vDir.y * fRemaining;
					vScaledA.z = vDir.z * fRemaining;

					vTo.x = vAt.x + vScaledA.x;
					vTo.y = vAt.y + vScaledA.y;
					vTo.z = vAt.z + vScaledA.z;

					vNext = vTo;
				}
				else
				{
					vScaledB.x = fStepLong * vDir.x;
					vScaledB.y = fStepLong * vDir.y;
					vScaledB.z = fStepLong * vDir.z;

					vStep.x = vAt.x + vScaledB.x;
					vStep.y = vAt.y + vScaledB.y;
					vStep.z = vAt.z + vScaledB.z;

					vNext = vStep;
				}

				sim.m_vStart = vAt;
				sim.m_vEnd   = vNext;

				sim.Setup();
			}

			nBlock = nRoot;
			pTop  = nStack;

			for (;;)
			{
				if (nBlock == WS_NODE_EMPTY || nBlock == WS_NODE_SOLID)
				{
					if (pTop == nStack)
						break;

					pTop--;
					nBlock = *pTop;
				}

				g_nPhysNodes++;

				pNode  = &g_pPBlock->m_pNodes[nBlock];
				pRec   = &pRecs[pNode->m_nPoly];
				pPlane = pRec->m_pOwner->m_pParent->m_pPlane;

				fDist = sim.m_vMid.z * pPlane->m_Normal.z +
				        sim.m_vMid.y * pPlane->m_Normal.y +
				        sim.m_vMid.x * pPlane->m_Normal.x - pPlane->m_Dist;

				// Written as an if/else chain rather than as guarded
				// `continue`s: all three arms end by descending, and the
				// shipped build keeps the descend at the *first* arm and
				// jumps back into it.  See bsp_SphereInside for the same
				// choice in a smaller function.
				if (fDist > sim.m_fReach)
				{
					nBlock = pNode->m_nChild[1];
				}
				else if (fDist < -sim.m_fReach)
				{
					nBlock = pNode->m_nChild[0];
				}
				else
				{
					nStart = sim.ClassifyPlane(pPlane, &sim.m_vStart, 0, 1);

					pPlane = pRec->m_pOwner->m_pParent->m_pPlane;

					nEnd = sim.ClassifyPlane(pPlane, &sim.m_vEnd, 1,
					                         nStart != 1);

					// FIX (b): TWO arms, each with its own TestPoly call and
					// its own written-out descend.  0x00419e05 cmp/jne to
					// 0x00419ea9; 0x00419e0d cmp 0x2; 0x00419ea9 cmp 0x1;
					// TestPoly at 0x00419e21 AND 0x00419eb9.
					if (nStart == nEnd)
					{
						if (nStart == WS_STRADDLE)
						{
							if (sim.TestPoly(pRec->m_pOwner,
							                 (WorldPoly*)pRec))
							{
								pState->m_nHits++;
							}

							// descend copy #1 -- 0x00419e34..0x00419e9a
							pPlane = pRec->m_pOwner->m_pParent->m_pPlane;

							fDist = sim.m_vStart.z * pPlane->m_Normal.z +
							        sim.m_vStart.y * pPlane->m_Normal.y +
							        sim.m_vStart.x * pPlane->m_Normal.x -
							        pPlane->m_Dist;

							if (fDist > WS_PLANE_SLOP)
								nSide = 1;
							else
								nSide = 0;

							if (pNode->m_nChild[nSide == 0] !=
							        WS_NODE_EMPTY &&
							    pNode->m_nChild[nSide == 0] !=
							        WS_NODE_SOLID)
							{
								*pTop = pNode->m_nChild[nSide == 0];
								pTop++;
							}

							nBlock = pNode->m_nChild[nSide];
						}
						else
						{
							// 0x00419e9f mov ax,[esi+ebx*2+0x2]
							nBlock = pNode->m_nChild[nStart];
						}
					}
					else
					{
						if (nStart == WS_INFRONT)
						{
							if (sim.TestPoly(pRec->m_pOwner,
							                 (WorldPoly*)pRec))
							{
								pState->m_nHits++;
							}
						}

						// descend copy #2 -- 0x00419ecc..0x00419f3a
						pPlane = pRec->m_pOwner->m_pParent->m_pPlane;

						fDist = sim.m_vStart.z * pPlane->m_Normal.z +
						        sim.m_vStart.y * pPlane->m_Normal.y +
						        sim.m_vStart.x * pPlane->m_Normal.x -
						        pPlane->m_Dist;

						if (fDist > WS_PLANE_SLOP)
							nSide = 1;
						else
							nSide = 0;

						if (pNode->m_nChild[nSide == 0] != WS_NODE_EMPTY &&
						    pNode->m_nChild[nSide == 0] != WS_NODE_SOLID)
						{
							*pTop = pNode->m_nChild[nSide == 0];
							pTop++;
						}

						nBlock = pNode->m_nChild[nSide];
					}
				}
			}

			if (nHitsAtStart != pState->m_nHits)
			{
				if (nDeflects != 0 && nHitsLast != pState->m_nHits)
				{
					nDeflects--;

					sim.DeflectOffHit(sim.m_pHitPoly, pState);

					nHitsLast = pState->m_nHits;

					continue;
				}

				if (nHitsLast != pState->m_nHits)
				{
					vTmp.x = g_vMoveBase.x + g_vMoveStart.x;
					vTmp.y = g_vMoveBase.y + g_vMoveStart.y;
					vTmp.z = g_vMoveBase.z + g_vMoveStart.z;

					sim.m_vEnd = vTmp;
				}

				break;
			}

			vScaled.x = fStepShort * vDir.x;
			vScaled.y = fStepShort * vDir.y;
			vScaled.z = fStepShort * vDir.z;

			vTmp.x = vAt.x + vScaled.x;
			vTmp.y = vAt.y + vScaled.y;
			vTmp.z = vAt.z + vScaled.z;

			vAt = vTmp;

			fRemaining = fRemaining - fStepShort;
		}

		vTmp.x = sim.m_vEnd.x - g_vMoveBase.x;
		vTmp.y = sim.m_vEnd.y - g_vMoveBase.y;
		vTmp.z = sim.m_vEnd.z - g_vMoveBase.z;

		g_vMoveEnd = vTmp;
	}
	}
	}
	}

	return 1;
}

// Can the object step up onto anything in this block? The same tree walk as
// the cylinder sweep, but looking only for upward-facing surfaces the object
// is moving into and is allowed to stand on. What it finds it steps onto
// there and then: the whole move is lifted, the box's height is cut by half
// the lift, and the geometry is rebuilt -- which is why the caller has to put
// the dims back afterwards either way. A surface marked no-step, or one that
// would lift the object further than the move descends, answers one instead
// and leaves everything alone.
//
// NEAR (1078 of 1080, the same 361 instructions, 9 blocks; lithtech 1430 of
// 1429). The layout came from three spellings: the radius tests as ONE
// if / else-if / else chain with no `continue`s (that puts the shared
// `nBlock = m_nChild[1]` tail on the FIRST arm, which Descend then jumps back
// to, as shipped 0x10004da3); the step guards as nested positive ifs with the
// descend code after them (not `goto Descend`); and the SDK operators for
// `vAny = m_Velocity + vVel`, `.Dot(vNormal)` and `vFlat * -fBest`. Walking
// the tree in the parameter itself (no nNode copy) put fBest in the dead
// nBlock slot as shipped. Left: shipped holds the node index in %bx, ours in
// %ax (allocation), and shipped's g_vMoveStart copy completes before the
// g_vMoveEnd loads (one store-scheduling block; operator/component spellings
// of both sums and statement order did not move it).
#ifdef NOLFSRC_MODERN
// M-P5-20: the highest point of a polygon over the rectangle x0..x1, z0..z1,
// or 0 when none of it is over it. The polygon is clipped to the rectangle
// seen from above (its height carried along each edge) and the highest of
// what is left is the answer: for a plane over a convex region, it is at one
// of the region's corners.
static int ws_PolyTopOver(WorldPolyOwner* pPoly, float x0, float x1,
                          float z0, float z1, float* pTop)
{
	LTVector vA[40], vB[40];
	LTVector *pIn = vA, *pOut = vB, *pSwap;
	uint32   nIn, nOut, i, nSide;
	float    fLimit, fSign, fCur, fPrev, fT;

	if (pPoly->m_nVertices < 3 || pPoly->m_nVertices > 32)
		return -1;   // not ours to answer: the retail lift

	for (i = 0; i < pPoly->m_nVertices; i++)
		vA[i] = *pPoly->m_Vertices[i].m_pPos;
	nIn = pPoly->m_nVertices;

	for (nSide = 0; nSide < 4 && nIn; nSide++)
	{
		// x >= x0, x <= x1, z >= z0, z <= z1, as fSign * (v - fLimit) >= 0
		fLimit = (nSide == 0) ? x0 : (nSide == 1) ? x1 : (nSide == 2) ? z0 : z1;
		fSign  = (nSide & 1) ? -1.0f : 1.0f;

		nOut  = 0;
		fPrev = fSign * (((nSide < 2) ? pIn[nIn - 1].x : pIn[nIn - 1].z) - fLimit);

		for (i = 0; i < nIn; i++)
		{
			const LTVector& vPrev = pIn[i ? i - 1 : nIn - 1];
			const LTVector& vCur  = pIn[i];

			fCur = fSign * (((nSide < 2) ? vCur.x : vCur.z) - fLimit);

			if ((fCur >= 0.0f) != (fPrev >= 0.0f))
			{
				fT = fPrev / (fPrev - fCur);
				pOut[nOut++] = vPrev + (vCur - vPrev) * fT;
			}
			if (fCur >= 0.0f)
				pOut[nOut++] = vCur;

			fPrev = fCur;
		}

		pSwap = pIn; pIn = pOut; pOut = pSwap;
		nIn = nOut;
	}

	if (!nIn)
		return 0;

	*pTop = pIn[0].y;
	for (i = 1; i < nIn; i++)
	{
		if (pIn[i].y > *pTop)
			*pTop = pIn[i].y;
	}
	return 1;
}
#endif

int ws_StairStep(uint16 nBlock, CBounceState* pState)
{
	uint16         nStack[WS_NODE_STACK];
	LTVector       vMax;
	LTVector       vMin;
	LTVector       vSave;
	LTVector       vAny;
	LTVector       vPoint;
	LTVector       vOrigNormal;
	LTVector       vPointA;
	LTVector       vVel;
	LTVector       vAtEnd;
	LTVector       vAtStart;
	LTVector       vCentre;
	LTVector       vFlat;
	LTVector       vNormal;
	WorldPolyRef*  pRecs;
	LTPlane*       pPlane;
	WSNode*        pNode;
	int            nResult;
	LTVector       vPush;
	float          fRise;
	WorldPolyRef*  pRec;
	uint16*        pTop;

	uint16          nChild;
	WorldPolyOwner* pOwner;
	CollisionInfo*  pInfo;
	LTObject*       pObj;
	int             nPoint;
	float           fDist, fBest, fScale;

	nResult = 0;

	if (g_vMoveStart.y > g_vMoveEnd.y)
		fRise = g_vMoveStart.y - g_vMoveEnd.y;
	else
		fRise = 0.0f;

	pRecs = g_pMoveState->m_pWorldBsp->GetPolyRefs();

	pTop  = nStack;

	for (;;)
	{
		if (nBlock == WS_NODE_EMPTY || nBlock == WS_NODE_SOLID)
		{
			if (pTop == nStack)
				break;

			pTop--;
			nBlock = *pTop;
		}

		pNode  = &g_pPBlock->m_pNodes[nBlock];
		pOwner = pRecs[pNode->m_nPoly].m_pOwner;
		pRec   = &pRecs[pNode->m_nPoly];
		pPlane = pOwner->m_pParent->m_pPlane;

		vCentre = g_BoxSphere.m_vPoint;

		// TERM ORDER IS NOT THE LEVER HERE, and this is worth reading before
		// anyone tries it again, because a normalised byte diff CANNOT SEE the difference:
		// it masks hex operands, so `flds X / fmuls X` compares
		// equal whichever vector component was loaded, and no mnemonic block
		// will ever mention it.
		//
		// The shipped site 0x10003b0b emits `fmuls 0x8(%eax)` (z) then
		// `fmuls 0x4(%eax)` (y). Ours emits 0x4 then 0x8. One reading took that
		// as our build reversing source order for a frame-local vector and
		// proposed swapping these two terms at four sites.
		//
		// MEASURED BY DISASSEMBLING BOTH SPELLINGS: swapping them changes
		// NOTHING. Source `z + y + x` and source `y + z + x` both compile to
		// `fmuls 0x4` then `fmuls 0x8`. VC6 re-associates the sum, so the
		// order written here never reaches the output and neither that
		// reading nor the obvious opposite one is right. Whatever
		// picks the shipped order, it is not this.
		fDist = vCentre.z * pPlane->m_Normal.z +
		        vCentre.y * pPlane->m_Normal.y +
		        vCentre.x * pPlane->m_Normal.x - pPlane->m_Dist;

		if (fDist > g_BoxSphere.m_fRadius)
		{
			nBlock = pNode->m_nChild[1];
		}
		else if (fDist < -g_BoxSphere.m_fRadius)
		{
			nBlock = pNode->m_nChild[0];
		}
		else
		{
			if (pOwner && pPlane->m_Normal.y > g_fStepMinNormalY)
			{
				vVel = g_pMoveResult->m_vVelocityAdd;

				pObj = g_pMoveState->m_pObject;

				vNormal = pPlane->m_Normal;

				vAny = pObj->m_Velocity + vVel;

				if (!(vAny.Dot(vNormal) > 0.0f) &&
				    w_IsPolyBlocking(pOwner))
				{
					fBest = WS_STEP_FAR;

					pPlane = pRec->m_pOwner->m_pParent->m_pPlane;

					pInfo = g_pMoveState->m_pInfo;

					pInfo->m_Plane = *pPlane;

					if (pPlane->m_Normal.y < g_fStepSlopeLimit)
					{
						pInfo->m_Plane.m_Normal.y = 0.0f;
						pInfo->m_Plane.m_Normal.Norm();

						vOrigNormal = pPlane->m_Normal;

						fScale = -1.0f /
						         (vOrigNormal.z * pInfo->m_Plane.m_Normal.z +
						          vOrigNormal.y * pInfo->m_Plane.m_Normal.y +
						          vOrigNormal.x * pInfo->m_Plane.m_Normal.x);

						for (nPoint = 0; nPoint < 4; nPoint++)
						{
							vPoint = g_SweepPoints[1][nPoint];

							fDist = (pInfo->m_Plane.m_Dist -
							         (vPoint.y * pPlane->m_Normal.y +
							          vPoint.z * pPlane->m_Normal.z +
							          vPoint.x * pPlane->m_Normal.x)) * fScale;

							if (fDist < fBest)
								fBest = fDist;
						}
					}
					else
					{
						for (nPoint = 0; nPoint < 4; nPoint++)
						{
							vPointA = g_SweepPoints[1][nPoint];

							fDist = vPointA.z * pInfo->m_Plane.m_Normal.z +
							        vPointA.y * pInfo->m_Plane.m_Normal.y +
							        vPointA.x * pInfo->m_Plane.m_Normal.x -
							        pInfo->m_Plane.m_Dist;

							if (fDist < fBest)
								fBest = fDist;
						}
					}

					g_pMoveState->m_pInfo->m_hObject = g_pMoveState->m_hObject;

					g_pMoveState->m_pInfo->m_hPoly =
					    g_pMoveState->m_pWorldBsp->MakeHPoly((WorldPoly*)pRec);

#ifdef NOLFSRC_MODERN
					// M-P5-20: a floor or a slope one can walk on lifts the
					// feet onto the highest point of it that is under them.
					// The lift was measured from the swept box's corners to
					// the polygon's plane, wherever the corners were, and the
					// plane goes on past the polygon: over the top edge of a
					// ramp, or a ridge between two slopes, the corners beyond
					// it were below that plane carried on, and the player was
					// stepped up onto it in mid-air -- shoved back and up,
					// falling again, over and over (retail does it too). And
					// along the slope's normal, that is backwards, off it.
					// Over the polygon throughout, this is retail's lift,
					// straight up.
					vPush.Init(0.0f, 0.0f, 0.0f);
					if (pPlane->m_Normal.y >= g_fStepSlopeLimit)
					{
						float fTop;
						int   nOver = ws_PolyTopOver(pOwner,
						                            g_SweepPoints[1][0].x, g_SweepPoints[1][2].x,
						                            g_SweepPoints[1][0].z, g_SweepPoints[1][2].z,
						                            &fTop);

						if (nOver >= 0)
						{
							fBest = (nOver && fTop > g_SweepPoints[1][0].y) ?
							        g_SweepPoints[1][0].y - fTop : 0.0f;
							vPush.y = -fBest;
						}
					}
#endif
					if (fBest < 0.0f)
					{
						vFlat = pInfo->m_Plane.m_Normal;

#ifdef NOLFSRC_MODERN
						if (vPush.y == 0.0f)
#endif
						vPush = vFlat * -fBest;

						if (pRec->m_pOwner->m_pParent->m_nFlags & WBSP_NOSTEP)
						{
							if (fRise < vPush.y)
							{
								nResult = 1;

								nBlock = pNode->m_nChild[1];

								continue;
							}
						}

						pState->m_nHits++;

						vSave = pState->m_vVelocityAdd;

						ws_HandleImpact(g_pMoveState->m_pInfo, pState,
						                g_pMoveState->m_pObject, g_pMoveState->m_hObject,
						                g_pMoveState->m_pWorldBsp, (WorldPoly*)pRec, 1);

						pState->m_vVelocityAdd = vSave;

						vAtStart = g_vMoveStart + vPush;

						g_vMoveStart = vAtStart;

						vAtEnd = g_vMoveEnd + vPush;

						g_vMoveEnd = vAtEnd;

						g_vMoveBase.y = g_vMoveBase.y - vPush.y * 0.5f;

						g_pMoveState->m_vDims.y =
						    g_pMoveState->m_vDims.y - vPush.y * 0.5f;

						ws_SetupSweptBox(&vMin, &vMax);

						g_pMoveResult->m_pStandingPoly = (WorldPoly*)pRec;

						fRise = fRise - vPush.y;
					}

					nBlock = pNode->m_nChild[1];

					continue;
				}
			}

			nChild = pNode->m_nChild[0];

			if (nChild != WS_NODE_EMPTY && nChild != WS_NODE_SOLID)
			{
				*pTop = nChild;
				pTop++;
			}

			nBlock = pNode->m_nChild[1];
		}
	}

	return nResult;
}

// The box sweep. Unlike the cylinder there is no marching: the box at each
// end of the move is projected onto every plane the tree offers, and a plane
// both ends straddle is a real candidate. Which of the seven projections is
// used comes off the polygon itself -- a plane lying along an axis needs no
// projection at all.
//
// A polygon the box is genuinely inside is answered one of two ways. On the
// first attempt at a move the contact is simply recorded. On a retry the box
// is pushed back along its own path, one step at a time, until it is outside
// -- fifty steps at most, and failing that the whole move is abandoned.
//
// NEAR, size-exact on server.dll (2134 = 2134 bytes, 617 = 617 instructions,
// no mnemonic difference). Read off the shipped bytes, each one moved it:
//   * the sphere reject is `pPlane->DistTo(g_BoxSphere.m_vPoint)`: the
//     by-value argument is the 12-byte copy at -0xa4 (0x10003ae4), not a
//     named vCentre in the upper frame;
//   * each re-projection has its own pair of position vectors (retry
//     -0x4c/-0x5c, hit -0x1c/-0x10) and the prologue's one is shared, which
//     is what makes the frame 0x9a0;
//   * no nResult: `return 1` / `return 0` carry the value in esi across the
//     one CountAdder destructor at 0x10004134, and `Failed:` has its own at
//     0x10004158 -- which also puts the counter at -0x68;
//   * the straddle arm is `if (nStart == WS_STRADDLE) {..} else {child}` with
//     positive guards round `if (bRetry) {push back} else {record}`, and the
//     front arm is a positive guard with the descend written out after it
//     (0x10003f36, 0x10003be3, 0x10003f45);
//   * the push-back loop is a `for` (one bottom test at 0x10003d0a), and has
//     its own pointer locals: pPts0 of the first test lives in the dead
//     nBlock slot, the loop's copy at -0x50, nTries in the nBlock slot;
//   * pPts1 is assigned before pPts0 (the -0x19c load comes first);
//   * the positions are `Init(g_vMoveBase.x + g_vMoveStart.x, ...)`: three
//     separate stores let VC6 keep g_vMoveBase.x on the stack across the
//     second one (`fld %st(0)`), which the shipped build never does;
//   * m_End.m_pProjected is stored before m_Start's (0x10003953).
// Left: 14 instructions. (a) pRecs sits at -0x50 where shipped has -0x40,
// which moves vRetryStart and the loop's pPts0 by 4. Neither declaration
// order, block scope nor the local's name changes that. (b) Both descend
// dot products emit y,z,x where shipped has z,y,x. No term order changes
// it. It follows the PROLOGUE's spelling: three component stores give z,y,x
// here, but the prologue then loses its shipped scheduling. Allocation, not
// structure.
int ws_MoveBox(uint16 nBlock, CBounceState* pState, int bRetry)
{
	uint16     nStack[WS_NODE_STACK];
	CPolyTest  test;
#ifdef LT_SERVER_TIMERS
	CountAdder cntAdder(&g_nPhysStepTime);
#endif
	LTVector   vBack;
	LTVector   vNorm;
	LTVector   vSaveStart;
	LTVector   vAt;
	LTVector   vRetryStart;
	LTVector   vRetryEnd;
	LTVector   vHitStart;
	LTVector   vHitEnd;

	WorldPolyRef* pRecs;
	WorldPolyRef* pRec;
	WSNode*       pNode;
	uint32        nProjected;
	uint16*       pTop;
	uint16        nNode;
	uint16        nChild;
	int           nStart, nEnd, nSide;
	// UNSIGNED: the bound test at 0x10003d0a is `jb`.
	uint32        nTries;
	WorldPolyOwner* pOwner;
	LTVector*     pPts0;
	LTVector*     pPts1;
	WorldPolyOwner* pPushOwner;
	LTVector*     pPushPts0;
	LTVector*     pPushPts1;
	LTPlane*      pPlane;
	float         fDist;

	vAt.Init(g_vMoveBase.x + g_vMoveStart.x, g_vMoveBase.y + g_vMoveStart.y,
	         g_vMoveBase.z + g_vMoveStart.z);

	test.m_Start.m_pPoints = g_SweepPoints[0];
	test.m_End.m_pPoints   = g_SweepPoints[1];

	test.m_Start.m_pSphere = &g_SphereAtDelta;
	test.m_End.m_pSphere   = &g_SphereAtOffset;

	test.m_End.m_pProjected   = &nProjected;
	test.m_Start.m_pProjected = &nProjected;

	test.m_Start.m_vMin = vAt - g_pMoveState->m_vDims;
	test.m_Start.m_vMax = vAt + g_pMoveState->m_vDims;

	vAt.Init(g_vMoveBase.x + g_vMoveEnd.x, g_vMoveBase.y + g_vMoveEnd.y,
	         g_vMoveBase.z + g_vMoveEnd.z);

	test.m_End.m_vMin = vAt - g_pMoveState->m_vDims;
	test.m_End.m_vMax = vAt + g_pMoveState->m_vDims;

	pRecs = g_pMoveState->m_pWorldBsp->GetPolyRefs();

	pTop  = nStack;
	nNode = nBlock;

	for (;;)
	{
		if (nNode == WS_NODE_EMPTY || nNode == WS_NODE_SOLID)
		{
			if (pTop == nStack)
			{
				return 1;
			}

			pTop--;
			nNode = *pTop;
		}

		g_nPhysNodes++;

		pNode  = &g_pPBlock->m_pNodes[nNode];
		pRec   = &pRecs[pNode->m_nPoly];
		pPlane = pRec->m_pOwner->m_pParent->m_pPlane;

		fDist = pPlane->DistTo(g_BoxSphere.m_vPoint);

		if (fDist > g_BoxSphere.m_fRadius)
		{
			nNode = pNode->m_nChild[1];
			continue;
		}

		if (fDist < -g_BoxSphere.m_fRadius)
		{
			nNode = pNode->m_nChild[0];
			continue;
		}

		nProjected = 1;

		test.m_End.m_pPlane   = pRec->m_pOwner->m_pParent->m_pPlane;
		test.m_Start.m_pPlane = test.m_End.m_pPlane;

		nStart = g_BoxProjectFns[pRec->m_nProject](&test.m_Start);
		nEnd   = g_BoxProjectFns[pRec->m_nProject](&test.m_End);

		if (nStart == nEnd)
		{
			if (nStart == WS_STRADDLE)
			{
				pOwner = pRec->m_pOwner;
				pPts1  = test.m_End.m_pPoints;
				pPts0  = test.m_Start.m_pPoints;

				if (w_IsPolyBlocking(pOwner) &&
				    !ws_IsBoxOutsidePoly(pOwner, pPts0, pPts1))
				{
					if (bRetry)
					{
						vBack.x = g_vMoveStart.x - g_vMoveEnd.x;
						vBack.y = g_vMoveStart.y - g_vMoveEnd.y;
						vBack.z = g_vMoveStart.z - g_vMoveEnd.z;

						vNorm = vBack;
						vNorm.Norm();

						ws_HandleImpact(g_pMoveState->m_pInfo, pState,
						                g_pMoveState->m_pObject, g_pMoveState->m_hObject,
						                g_pMoveState->m_pWorldBsp, (WorldPoly*)pRec, 1);

						pState->m_nHits++;

						vSaveStart = g_vMoveStart;

						for (nTries = 0; nTries < WS_MAX_BOX_PUSHES; nTries++)
						{
							pPushOwner = pRec->m_pOwner;
							pPushPts1  = test.m_End.m_pPoints;
							pPushPts0  = test.m_Start.m_pPoints;

							if (!w_IsPolyBlocking(pPushOwner))
								break;

							if (ws_IsBoxOutsidePoly(pPushOwner, pPushPts0, pPushPts1))
								break;

							g_vMoveStart.x = vNorm.x + g_vMoveStart.x;
							g_vMoveStart.y = vNorm.y + g_vMoveStart.y;
							g_vMoveStart.z = vNorm.z + g_vMoveStart.z;

							g_vMoveEnd.x = vNorm.x + g_vMoveEnd.x;
							g_vMoveEnd.y = vNorm.y + g_vMoveEnd.y;
							g_vMoveEnd.z = vNorm.z + g_vMoveEnd.z;

							if (!ws_ShiftBox())
								return 0;

						}

						g_vMoveStart = vSaveStart;

						if (nTries == WS_MAX_BOX_PUSHES)
						{
							g_vMoveEnd.x = vSaveStart.x;
							g_vMoveEnd.y = vSaveStart.y;
							g_vMoveEnd.z = vSaveStart.z;

							goto Failed;
						}

						vRetryStart.Init(g_vMoveBase.x + g_vMoveStart.x, g_vMoveBase.y + g_vMoveStart.y,
						         g_vMoveBase.z + g_vMoveStart.z);

						test.m_Start.m_vMin = vRetryStart - g_pMoveState->m_vDims;
						test.m_Start.m_vMax = vRetryStart + g_pMoveState->m_vDims;

						vRetryEnd.Init(g_vMoveBase.x + g_vMoveEnd.x, g_vMoveBase.y + g_vMoveEnd.y,
						         g_vMoveBase.z + g_vMoveEnd.z);

						test.m_End.m_vMin = vRetryEnd - g_pMoveState->m_vDims;
						test.m_End.m_vMax = vRetryEnd + g_pMoveState->m_vDims;
					}
					else
					{
						g_pMoveState->m_pInfo->m_Plane =
						    *pRec->m_pOwner->m_pParent->m_pPlane;

						g_pMoveState->m_pInfo->m_hObject = g_pMoveState->m_hObject;

						g_pMoveState->m_pInfo->m_hPoly =
						    g_pMoveState->m_pWorldBsp->MakeHPoly((WorldPoly*)pRec);

						pState->m_nHits++;
					}
				}

				pPlane = pRec->m_pOwner->m_pParent->m_pPlane;

				fDist = g_vMoveStart.z * pPlane->m_Normal.z +
				        g_vMoveStart.y * pPlane->m_Normal.y +
				        g_vMoveStart.x * pPlane->m_Normal.x - pPlane->m_Dist;

				if (fDist > WS_PLANE_SLOP)
					nSide = 1;
				else
					nSide = 0;

				nChild = pNode->m_nChild[nSide == 0];

				if (nChild != WS_NODE_EMPTY && nChild != WS_NODE_SOLID)
				{
					*pTop = nChild;
					pTop++;
				}

				nNode = pNode->m_nChild[nSide];
			}
			else
			{
				nNode = pNode->m_nChild[nStart];
			}
		}
		else
		{
			if (nStart == WS_INFRONT && w_IsPolyBlocking(pRec->m_pOwner))
			{
				if (!ws_HitPoly((WorldPoly*)pRec, pState, &test))
					goto Failed;

				vHitStart.Init(g_vMoveBase.x + g_vMoveStart.x, g_vMoveBase.y + g_vMoveStart.y,
				         g_vMoveBase.z + g_vMoveStart.z);

				test.m_Start.m_vMin = vHitStart - g_pMoveState->m_vDims;
				test.m_Start.m_vMax = vHitStart + g_pMoveState->m_vDims;

				vHitEnd.Init(g_vMoveBase.x + g_vMoveEnd.x, g_vMoveBase.y + g_vMoveEnd.y,
				         g_vMoveBase.z + g_vMoveEnd.z);

				test.m_End.m_vMin = vHitEnd - g_pMoveState->m_vDims;
				test.m_End.m_vMax = vHitEnd + g_pMoveState->m_vDims;

				nNode = pNode->m_nChild[1];

				continue;
			}

			pPlane = pRec->m_pOwner->m_pParent->m_pPlane;

			fDist = g_vMoveStart.z * pPlane->m_Normal.z +
			        g_vMoveStart.y * pPlane->m_Normal.y +
			        g_vMoveStart.x * pPlane->m_Normal.x - pPlane->m_Dist;

			if (fDist > WS_PLANE_SLOP)
				nSide = 1;
			else
				nSide = 0;

			nChild = pNode->m_nChild[nSide == 0];

			if (nChild != WS_NODE_EMPTY && nChild != WS_NODE_SOLID)
			{
				*pTop = nChild;
				pTop++;
			}

			nNode = pNode->m_nChild[nSide];
		}
	}

Failed:
	return 0;
}
